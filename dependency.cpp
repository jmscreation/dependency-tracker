#include <cstdlib>

#include <iostream>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

typedef const std::vector<std::string>& Parameters; // parameters type

/* Global parameters for applciation */

struct GlobalParameters {
    static bool ignoreCurrentPath, ignoreDependencyFileHeader, ignorePull, cleanRepo, verboseLog;

    GlobalParameters() = delete;
    ~GlobalParameters() = delete;
};

bool GlobalParameters::ignoreCurrentPath = false;
bool GlobalParameters::ignoreDependencyFileHeader = false;
bool GlobalParameters::ignorePull = false;
bool GlobalParameters::cleanRepo = false;
bool GlobalParameters::verboseLog = false;

/* Forward Declaration For Command Callbacks */
bool displayHelp(Parameters params={});
bool searchForDependency(Parameters params={});
bool updateDependencies(Parameters params={});

/* Command Syntax */
struct Command {
    // command and callback function
    std::string cmd;
    std::function<bool(Parameters parameters)> callback;
    std::vector<std::string> defaultParameters;

    // help and syntax
    std::vector<std::string> parameterNames;
    std::string help;
};


/* Command Line Arguments Stored In Vector */

std::vector<std::string> args; // program arguments
std::string cliarg; // program path (argument 0)

const std::vector<Command>& commands = {
    // switches

    {"verbose", [](auto params){ GlobalParameters::verboseLog = true; return true; }, {}, {}, "enable verbose logging"},
    {"ignore-header", [](auto params){ GlobalParameters::ignoreDependencyFileHeader = true; return true; }, {}, {}, "ignore strict file header when searching for dependencies"},
    {"ignore-pull", [](auto params){ GlobalParameters::ignorePull = true; return true; }, {}, {}, "skip updating repositories / do not fetch or pull existing libraries"},
    {"ignore-curpath", [](auto params){ GlobalParameters::ignoreCurrentPath = true; return true; }, {}, {}, "ignore searching for dependency files within current working directory"},
    {"clean", [](auto params){ GlobalParameters::cleanRepo = true; return true; }, {}, {}, "clean up all libraries and force pull the latest from the branch"},

    // terminating commands

    {"help", &displayHelp, {}, {}, "displays this list of commands"},
    {"list", &searchForDependency, {"./libraries", "dependency.txt"}, {"library-path", "dependency-list-filename"}, "searches for dependencies in library directory and current working directory"},
    {"update", &updateDependencies, {"./libraries", "dependency.txt"}, {"library-path", "dependency-list-filename"}, "update all dependencies and clone to library directory"}

};

void initArguments(int argc, const char** argv) {
    cliarg = argv[0];
    args.reserve(argc-1);
    for(int i=1; i<argc; ++i) args.push_back(argv[i]);
}

bool FindArg(const std::string& arg){
    return (std::find(args.begin(), args.end(), arg) != args.end());
}
bool FindParam(const std::string& arg, std::string& param, int ind=1){
    auto it = std::find(args.begin(), args.end(), arg);
    if(it == args.end()) return false;
    for(int i=0; i < ind; i++) if(++it == args.end()) return false;
    if((*it)[0] == '-'){ // denotes that the parameter might actually be a command
        if(std::find_if(commands.begin(), commands.end(),
                        [&](const Command& cmd){ return ("-" + cmd.cmd) == *it; }) != commands.end())
            return false; // if the command exists in the database, then this is not a parameter
    }
    param = *it;
    it->clear(); // consume parameter
    return true;
}

// ----------------------------------------

/* System Execution */

int execute(const std::string& command, const std::vector<std::string>& cargs){
    std::string cli = command;
    for(const std::string& arg : cargs) cli += " " + arg;
    
    if(GlobalParameters::verboseLog) std::cout << cli << "\n";
    int rval = system(cli.data());

    return rval;
}

// ----------------------------------------

/* Dependency System */

struct Dependency {
    std::string url;
    std::string branch;
};

bool validateGit() {
    if(GlobalParameters::verboseLog) std::cout << "Determining git version...\n";

    if(execute("git", {"--version"} ) != 0){
        std::cout << "System could not find git - Please install git and configure your system PATH to continue\n";
        return false;
    }
    return true;
}

std::string readLine(std::ifstream& file) { // returns a line with content, or false if eof
    if(!file.is_open()) return "";
    
    bool comment = false;
    std::string data;
    do {
        char c;
        if(!file.readsome(&c, 1)) break;
        if(c == '\r') continue; // windows return sequence
        if(c == '\n'){
            if(!data.size()) continue;
            if(comment){
                comment = false;
            }
            break;
        }

        if(c == '<' || c == '>' || c == '"' || c == '|'){
            comment = true;
        }

        if(comment) continue;
        data += c;
    } while(1);

    return data;
}

bool validateDependency(std::ifstream& file) {
    if(!file.is_open()) return false;
    
    file.seekg(file.beg); // read from beginning of file

    std::string header = readLine(file);
    bool valid = (header == "#DEPENDENCIES");

    if(!valid && GlobalParameters::ignoreDependencyFileHeader){
        std::cout << "Warning: Including a dependency list which might be invalid\n";
        valid = true;
    }
    return valid;
}

std::string getLibraryName(const Dependency& dep) {
    std::string allowedChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_~!$&'()*+,;=:@";
    std::string url = dep.url;

    // strip tags
    if(std::find(url.begin(), url.end(), '#') != url.end()){
        url = url.substr(0, url.find_last_of('#'));
    }

    // strip ?s
    if(std::find(url.begin(), url.end(), '?') != url.end()){
        url = url.substr(0, url.find_last_of('?'));
    }
    
    // strip invalid ending characters
    if(std::find(allowedChars.begin(), allowedChars.end(), url.back()) == allowedChars.end()){
        url.pop_back(); // character not allowed
    }

    // strip url beginning
    url = url.substr(url.find_last_of('/') + 1);

    return url + "-" + dep.branch;
}

bool readDependency(Dependency& dep, std::ifstream& file) {
    if(!file.is_open()) return false;

    std::string line = readLine(file);
    if(line.empty()) return false; // eof
    
    dep.url.clear();
    dep.branch.clear();

    int step = 0;
    for(char c : line){
        if(c == ' ' || c == '\t'){
            if(dep.url.size()) step = 1;
            continue;
        }
        switch(step){
            case 0:{
                dep.url += c;
            } break;
            case 1:{
                dep.branch += c;
            } break;
        }
    }

    return true;
}

std::vector<std::string> findDependencyFiles(const std::string& libdir, const std::string& filename) {
    std::vector<std::string> paths;
    if(fs::is_regular_file(fs::path(libdir))){
        if(GlobalParameters::verboseLog) std::cout << "Library path is not valid: " << libdir << "\n";
        return {};
    }
    if(!fs::is_directory(fs::path(libdir))){
        fs::create_directories(fs::path(libdir));
    }

    try {
        fs::directory_iterator wdir(libdir);
        for(fs::path p : wdir){
            if(!fs::is_directory(p)) continue;
            std::string f = p.string() + "/" + filename;
            if(!fs::is_regular_file(fs::path(f))) continue;

            if(GlobalParameters::verboseLog) std::cout << "Found dependency file " << f << "\n";

            std::ifstream file(f, std::ios::binary | std::ios::in);
            if(validateDependency(file)) paths.push_back(f);
            file.close();
        }

    } catch(std::exception e) {
        std::cout << "Failed to find dependency list!\n";
    }

    return paths;
}

std::vector<Dependency> getDependencies(const std::string& path) {
    std::vector<Dependency> dependencies;

    std::ifstream file(path, std::ios::binary | std::ios::in);
    if(!validateDependency(file)){
        std::cout << "\tinvalid dependency file\n";
    } else {
        Dependency curDep;
        while(readDependency(curDep, file)){
            dependencies.push_back(curDep);
        }
    }
    file.close();

    return dependencies;
}

void mergeDependencies(std::vector<Dependency>& dest, std::vector<Dependency>&& src) {

    for(Dependency& dep : src){
        if(std::find_if(dest.begin(), dest.end(),
                    [&](const Dependency& d){ return (d.branch == dep.branch && d.url == dep.url); }) != dest.end()){
            continue; // duplicate found -- skip dependency merge
        }

        dest.emplace_back(std::move(dep));
    }
}

bool populateDependencyList(std::vector<Dependency>& dpList, const std::string& libdir, const std::string& depfname) {
    if(fs::is_regular_file(fs::path(libdir))){
        if(GlobalParameters::verboseLog) std::cout << "Library path is not valid: " << libdir << "\n";
        return {};
    }
    if(!fs::is_directory(fs::path(libdir))){
        fs::create_directories(fs::path(libdir));
    }

    std::string dir = fs::canonical(libdir).string();

    auto list = findDependencyFiles(dir, depfname);
    if(!GlobalParameters::ignoreCurrentPath){
        if(GlobalParameters::verboseLog) std::cout << "Searching for current path dependencies...\n";

        std::string localpath = fs::current_path().string() + "/" + depfname;

        std::ifstream file(localpath, std::ios::binary | std::ios::in);
        if(validateDependency(file)) list.emplace_back(std::move(localpath)); // found local dependency file
        file.close();
    }

    for(std::string p : list){
        mergeDependencies(dpList, getDependencies(p));
    }

    if(dpList.size()) return true;
    
    std::cout << "No dependencies found!\n";
    return false; // no dependencies
}

// ----------------------------------------


int main(int argc, const char** argv) {
    initArguments(argc, argv);

    for(const Command& cmd : commands){
        std::string c = "-" + cmd.cmd;
        std::vector<std::string> params;
        if(cmd.parameterNames.size()) params.resize(cmd.parameterNames.size());

        if(FindArg(c)){
            int n=1;
            for(std::string& p : params){
                if(!FindParam(c, p, n)){
                    p = cmd.defaultParameters[n-1];
                }
                n++;
            }

            if(!cmd.callback(params)) return 1;
        }
    }

    // Default Run
    displayHelp();

    return 0;
}





// ----------------------------------------

/* Command Line Callback Functions */

bool displayHelp(Parameters params) {
    std::cout << "-----------------------------------\n"
              << "------ Git Dependency Loader ------\n"
              << "-----------------------------------\n"
              << "   === Command Line Help ===\n"
              << "Usage:        " << fs::path(cliarg).filename().string() << "\n";
    
    size_t lcmdlen = std::accumulate(commands.begin(), commands.end(), 16,
                [](size_t len, const Command& cmd) -> size_t {
                    size_t clen = cmd.cmd.size() + 4 + std::accumulate(cmd.parameterNames.begin(), cmd.parameterNames.end(), 0, [](size_t len, const std::string& params) -> size_t { return len + params.size() + 3; });
                    return clen > len ? clen : len;
                });

    for(const Command& cmd : commands){
        size_t plen = cmd.cmd.size() + 3;
        std::cout << "  -" << cmd.cmd;
        for(const std::string& para : cmd.parameterNames){
            std::cout << " <" << para << ">";
            plen += para.size() + 3;
        }
        std::cout << std::string(std::max(int64_t(lcmdlen - plen), int64_t(1)), ' ') << ";" << cmd.help << "\n";
    }
    std::cout << "-----------------------------------\n";

    return false; // close application when complete
}

bool searchForDependency(Parameters params) {
    if(!validateGit()) return false;
    
    std::vector<Dependency> list;
    if(populateDependencyList(list, params[0], params[1])){
        std::string dir = fs::canonical(params[0]).string(); // make canonical directory path
        std::cout << "Found dependencies:\n";
        for(Dependency& dep : list){
            std::string libname = getLibraryName(dep),
                        libpath = fs::path(dir + "/" + libname).make_preferred().string();
            std::cout << " " << libname << "\t" << dep.url << " [" << dep.branch << "] -> " << libpath << "\n";
        }
    }

    return false; // close application when complete
}

bool updateDependencies(Parameters params) {
    if(!validateGit()) return false;

    bool recursing = false, newLibrary = false;

    do {
        newLibrary = false; // reset found-new-library state
        std::vector<Dependency> list;
        if(populateDependencyList(list, params[0], params[1])){
            std::string dir = fs::canonical(params[0]).string(); // make canonical directory path

            for(Dependency& dep : list){
                std::string libname = getLibraryName(dep),
                            libpath = fs::path(dir + "/" + libname).make_preferred().string();
                if(fs::is_directory(fs::path(libpath))){ // library exists
                    if(!GlobalParameters::ignorePull && !recursing){ // only fetch if user did not directly ignore AND this is the first iteration
                        if(GlobalParameters::cleanRepo){
                            if(GlobalParameters::verboseLog) std::cout << "Cleaning repository " << libname << "...\n";
                            execute("git", {"-C", libpath, "reset", "--hard", "HEAD"} );
                        }
                        if(GlobalParameters::verboseLog) std::cout << "Fetch/Pull existing library: " << libname << " in " << dir << "\n";
                        execute("git", {"-C", libpath, "pull"} );
                    }
                } else {
                    if(GlobalParameters::verboseLog) std::cout << "Cloning dependency library: " << libname << " to " << libpath << "\n";
                    execute("git", {"clone", "-b", dep.branch, "--recurse-submodules", dep.url, libpath} );
                    newLibrary = true; // new library found
                }
            }
        }
        recursing = true; // after first iteration, sequential iterations will be recursive
    } while(newLibrary); // keep looping until a new library is not longer found

    return false; // close application when complete
}