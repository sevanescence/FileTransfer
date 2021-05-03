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
        // https://stackoverflow.com/a/478960
        // requires gnu++ stdlib
        static std::string exec(char const *cmd) {
            std::array<char, 128> buffer;
            std::string res;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
            if (! pipe) {
                throw std::runtime_error("popen() failed!");
            }
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                res += buffer.data();
            }
            return res;
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
    std::string id;
};

std::ostream &operator<<(std::ostream &os, RemoteFilePipelineMeta &meta) {
    os << "remote=\"" << meta.remotePath << "\"\n" << "local=\"" << meta.localTarget << "\"\n";
    return os;
}

std::istream &operator>>(std::istream &is, RemoteFilePipelineMeta &meta) {
    std::map<std::string, std::string &> props
    {
        { "remote", meta.remotePath },
        { "local", meta.localTarget }
    };

    std::regex cfgRgx("[A-z]+?(?=(\\s+?)?=)");
    std::regex cfgItemStrRgx("\"(.+?)\"");

    std::string line;
    std::string key;
    std::string val;
    std::smatch sm;
    while (is >> line) {
        std::regex_search(line, sm, cfgRgx);
        key = sm[0];
        std::regex_search(line, sm, cfgItemStrRgx);
        val = sm[1];
        std::map<std::string, std::string &>::iterator it = props.find(key);
        if (it != props.end()) {
            it->second = val;
        }
    }
}

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
        char const * build(std::vector<RemoteFilePipelineMeta> vec, char const *auxPath) {
            char args[CMD_BUFSIZE];
            // used std::string for this because it would take far more code without it.

            std::string cppstrtmp;
            cppstrtmp.append(" ").append(source).append(":\"");
            std::vector<RemoteFilePipelineMeta>::iterator it;
            for (it = vec.begin(); it != vec.end(); ++it) {
                cppstrtmp.push_back(' ');
                cppstrtmp.append(it->remotePath);
            }
            cppstrtmp.append("\" ").append(std::string(auxPath)).push_back(' ');

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

class FileListMeta {
    private:
        int length;
        char **files;
    public:
        FileListMeta(int _length, char **_files) {
            length = _length;
            files = _files;
        }
        int size() {
            return length;
        }
        char ** getFiles() {
            return files;
        }
        friend std::ostream &operator<<(std::ostream &os, FileListMeta const &flm);
};

std::ostream &operator<<(std::ostream &os, FileListMeta const &flm) {
    os << "{ length: " << flm.length << ", list_ptr: " << flm.files << " }";
}

class DataFolderManager {
    private:
        char *DATA_FOLDER;
        char const SETTINGS_CONFIG[13] = "settings.cfg";
        char const PIPELINE_CONFIG_FOLDER[10] = "pipelines";
        char const AUXILIARY_FOLDER[4] = "tmp";
        char *pmalloc(char const *__f) {
            char *tmp = (char *) malloc(MAX_PATH + 1);
            memcpy(tmp, DATA_FOLDER, MAX_PATH);
            strcat(tmp, separatorConst());
            strcat(tmp, __f);
            return tmp;
        }
    public:
        DataFolderManager(char *__DATA_FOLDER) {
            DATA_FOLDER = __DATA_FOLDER;
        }
        char *getDataFolder() {
            return DATA_FOLDER;
        }
        char *getSettingsConfigPath() {
            return pmalloc(SETTINGS_CONFIG);
        }
        char *getPipelineConfigFolder() {
            return pmalloc(PIPELINE_CONFIG_FOLDER);
        }
        char *getAuxiliaryFolder() {
            return pmalloc(AUXILIARY_FOLDER);
        }
        FileListMeta getPipelineFileList() {
            char cmd[256] = "dir ";
            strcat(cmd, getPipelineConfigFolder());
            strcat(cmd, " /b | findstr \"\\.cfg$\"");
            std::string __exec_stdout = WindowsProcessDelegate::exec(cmd);

            // get newlines in string
            int nls = 0;
            int pivot = 0;
            std::vector<std::string> fileVector;
            for (int i = 0; i < __exec_stdout.length(); i++) {
                if (__exec_stdout.at(i) == '\n') {
                    nls++;
                    std::string tmp;
                    for (int j = pivot; j < i; j++) {
                        tmp.push_back(__exec_stdout.at(j));
                    }
                    fileVector.push_back(tmp);
                    pivot = i + 1;
                }
            }
            // nls = number of newlines in stdout, length of file list.
            // 28 = 24 pipeline name length limit + .cfg (4 characters)
            char **files = (char **) malloc(sizeof(char *) * nls);
            for (int i = 0; i < fileVector.size(); i++) {
                files[i] = (char *) malloc(sizeof(char) * 28 + 1);
                memcpy(files[i], fileVector.at(i).c_str(), 28);
            }
            return FileListMeta(nls, files);
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
        int const CIPHER = '1';
        int const SSH_CONFIG = '2';
        int const IDENTITY_FILE = '3';
        int const LIMIT = '4';
        int const SSH_OPTION = '5';
        int const PORT = '6';
        int const PROGRAM = '7';
        int const SOURCE = '8';
    }
    namespace Pipelines {
        int const NEW_PIPELINE = 'n';
        int const DELETE_PIPELINE = 'd';
    }
}

class MainProcess {
    private:
        OpenSSHHandler *sshHandler;
        SCPArgumentsBuilder *builder;
        DataFolderManager *dataFolderManager;
        std::vector<RemoteFilePipelineMeta> *pipelines;
    public:
        MainProcess(OpenSSHHandler &_handler,
                    SCPArgumentsBuilder &_builder,
                    DataFolderManager &_manager,
                    std::vector<RemoteFilePipelineMeta> &_pipelines) {
            this->sshHandler = &_handler;
            this->builder = &_builder;
            this->dataFolderManager = &_manager;
            this->pipelines = &_pipelines;
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
        std::vector<RemoteFilePipelineMeta> &getPipelines() {
            return *pipelines;
        }
};

int MainMenu(MainProcess &process);
int Settings(MainProcess &process);
int Settings(MainProcess &process, int __t); // callback to avoid output stream overlap
int Pipelines(MainProcess &process);
int Pipelines(MainProcess &process, int __t);
int RunPipelines(MainProcess &proces);

// https://stackoverflow.com/a/478960
// requires gnu++ stdlib (moved to WindowsProcessDelegate::exec(std::basic_char<string>))
// std::string exec(char const *cmd) {
//     std::array<char, 128> buffer;
//     std::string res;
//     std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
//     if (! pipe) {
//         throw std::runtime_error("popen() failed!");
//     }
//     while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
//         res += buffer.data();
//     }
//     return res;
// }

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

    if (! FileExists(dataFolderManager.getPipelineConfigFolder())) {
        _mkdir(dataFolderManager.getPipelineConfigFolder());
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

    std::vector<RemoteFilePipelineMeta> pipelines;
    // TODO: get all pipelines in folder.
    // RemoteFilePipelineMeta cfg parser
    FileListMeta fileList = dataFolderManager.getPipelineFileList();
    std::string path = dataFolderManager.getPipelineConfigFolder();
    for (int i = 0; i < fileList.size(); i++) {
        path.push_back(separator());
        path += fileList.getFiles()[i];
        
        std::ifstream is(path);
        RemoteFilePipelineMeta meta;
        is >> meta;
        meta.id = std::string(std::string(fileList.getFiles()[i]), 0, strlen(fileList.getFiles()[i]) - 4);
        pipelines.push_back(meta);

        is.close();

        path.assign(dataFolderManager.getPipelineConfigFolder()); // reset path string
    }
    // std::system("pause");
    // ProgramExit(0);

    MainProcess process(sshHandler, builder, dataFolderManager, pipelines);
    // std::string str("scp ");
    // str.append(builder.source).append(":/home/mmiyamoto/assignments/essay.txt");
    // std::cout << builder.build(pipelines, "C:\\Users\\damia\\") << std::endl;

    char const *auxFolder = dataFolderManager.getAuxiliaryFolder();
    std::string cmd = "scp";
    cmd.append(std::string(builder.build(pipelines, auxFolder)));
    std::cout << cmd << std::endl;

    std::system("pause");
    
    return 0;

    int option;
    for (;;) {
        option = MainMenu(process);
        switch (option) {
            case Options::MainMenu::SETTINGS:
                Settings(process, 1);
                break;
            case Options::MainMenu::PIPELINES:
                Pipelines(process, 1);
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

    int ch = std::cin.get() - '0';
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

void SettingsOptionReassign(MainProcess &process, int setting) {
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

int Settings(MainProcess &process, int __t) {
    if (__t) {
        std::cin.clear();
        std::cin.ignore(123, '\n');
    }
    return Settings(process);
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
    std::cout << "[]\n[] Type any setting to reassign.\n[] Press ENTER to go back.\n[]\n[] > ";
    char option = std::cin.get();
    if (option == '\n') {
        return 0;
    }
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
            return Settings(process);
        case Options::System::BACKSPACE:
            return 0;
        default:
            return Settings(process);
    }
}

// []=>=>=>[] PIPELINES []=>=>=>[]
// [] 1. essay
// [] 2. math_answers
// [] 
// [] n. Create new pipeline
// [] d <name>. Delete pipeline
// [] Press ENTER to go back
// []
// [] > 

bool PipelineMetaVectorContainsID(std::vector<RemoteFilePipelineMeta> vec, std::string s) {
    std::vector<RemoteFilePipelineMeta>::iterator it;
    for (it = vec.begin(); it != vec.end(); ++it) {
        if (! it->id.compare(s)) {
            return true;
        }
    }
    return false;
}

int CreateNewPipeline(MainProcess &process) {
    std::system("cls");
    std::cout << "Create new pipeline.\n\n";
    
    RemoteFilePipelineMeta meta;
    ID_Assign:
    std::cout << "Enter new pipeline ID: ";
    std::string s;
    std::getline(std::cin, s);
    if (PipelineMetaVectorContainsID(process.getPipelines(), s)) {
        std::cout << "Error: ID already exists." << std::endl;
        goto ID_Assign;
    }
    if (s.size() > 24) {
        std::cout << "Error: ID must be no more than 24 characters." << std::endl;
        goto ID_Assign;
    }
    meta.id.assign(s);
    std::cout << "Enter remote path: ";
    std::getline(std::cin, s);
    meta.remotePath.assign(s);

    std::cout << "Enter target path: ";
    std::getline(std::cin, s);
    meta.localTarget.assign(s);

    process.getPipelines().push_back(meta);

    std::string path(process.getDataFolderManager().getPipelineConfigFolder());
    path.push_back(separator());
    path.append(meta.id).append(".cfg");
    std::ofstream os(path);
    os << meta;
    os.close();

    std::cout << meta.id << " added!" << std::endl;
    std::system("pause");
    return Pipelines(process);
}

void DeletePipeline(MainProcess &process, std::string id) {
    std::vector<RemoteFilePipelineMeta>::iterator it;
    for (it = process.getPipelines().begin(); it != process.getPipelines().end(); ++it) {
        if (it->id.compare(id) == 0) {
            process.getPipelines().erase(it);
            break;
        }
    }
    std::string cfg("del ");
    cfg.append(process.getDataFolderManager().getPipelineConfigFolder());
    cfg.push_back(separator());
    cfg.append(id).append(".cfg");
    WindowsProcessDelegate::exec(cfg.c_str());
    std::cout << "[] " << id << " deleted." << std::endl;
}

RemoteFilePipelineMeta GetPipelineByID(std::vector<RemoteFilePipelineMeta> vec, std::string id) {
    std::vector<RemoteFilePipelineMeta>::iterator it;
    for (it = vec.begin(); it->id.compare(id); ++it);
    return *it;
}

int EditPipeline(MainProcess &process, RemoteFilePipelineMeta meta) {
    std::system("cls");

    std::cout << "Editing " << meta.id << std::endl;
    std::cout << "Remote: \"" << meta.remotePath << "\"\n";
    std::cout << "Local: \"" << meta.localTarget << "\"\n";
    std::cout << "Enter a setting to change (the first letter works too).\nPress ENTER to go back.\n\n> ";
    
    Setting_Change:
    bool remote; // whether to change remote or local
    std::string option;
    std::getline(std::cin, option);
    if (option.length() == 0) return 0;
    switch (option.at(0)) {
        case 'R':
        case 'r':
            remote = true;
            break;
        case 'L':
        case 'l':
            remote = false;
            break;
        default:
            std::cout << "Pick either (R)emote or (L)ocal" << std::endl;
            goto Setting_Change;
    }

    std::cout << "Set new path for " << (remote ? "remote" : "target") << ": ";
    std::getline(std::cin, option);
    remote ? meta.remotePath = option : meta.localTarget = option;

    std::string path(process.getDataFolderManager().getPipelineConfigFolder());
    path.push_back(separator());
    path.append(meta.id).append(".cfg");

    std::cout << "Setting saved!" << std::endl;
    std::system("pause");
    return 0;
}

int Pipelines(MainProcess &process, int __t) {
    if (__t) {
        std::cin.clear();
        std::cin.ignore(123, '\n');
    }
    return Pipelines(process);
}

int Pipelines(MainProcess &process) {
    std::system("cls");
    std::cout << "[]=>=>=>[] PIPELINES []=>=>=>[]" << std::endl;
    std::vector<RemoteFilePipelineMeta>::iterator it = process.getPipelines().begin();
    int i = 1;
    while (it != process.getPipelines().end()) {
        std::cout << "[] " << i++ << ". [" << it->id << "]\n";
        std::cout << "[] remote=\"" << it->remotePath << "\"\n";
        std::cout << "[] local=\"" << it->localTarget << "\"\n[]" << std::endl;
        it++;
    }
    std::cout << "[] n. Create new pipeline\n[] d. <name> Delete pipeline (ex. \"d. essay\")\n[] Press ENTER to go back\n[]\n";
    std::cout << "[] > ";

    RemoteFilePipelineMeta meta;
    std::string option;
    std::string id;
    int pos;
    char __o;
    
    std::getline(std::cin, option);
    if (option.size() == 0) return 0;
    switch (option.at(0)) {
        case Options::Pipelines::NEW_PIPELINE:
            return CreateNewPipeline(process);
            break;
        case Options::Pipelines::DELETE_PIPELINE:
            id = std::string(option, 3);
            if (! PipelineMetaVectorContainsID(process.getPipelines(), id)) {
                std::cout << "Pipeline " << id << " does not exist." << std::endl;
            } else {
                std::cout << "[] Are you sure? (Y/n): ";
                std::cin >> __o;
                if (__o == 'Y') DeletePipeline(process, id);
                std::cin.clear();
                std::cin.ignore(123, '\n');
            }
            std::system("pause");
            break;
        default:
            if (std::regex_match(option, std::regex("^[0-9]{1,4}$"))) {
                try {
                    pos = std::stoi(option);
                    meta = process.getPipelines().at(pos - 1); // good code yes
                    EditPipeline(process, meta);
                } catch (...) {
                    std::cout << "Pipeline out of range." << std::endl;
                    std::system("pause");
                }
            }
            break;
    }
    return Pipelines(process);
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
