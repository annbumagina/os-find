#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

void wrong_usage(string message) {
    cout << message << "\n";
    cout << "Usage: find <dir-path> [-inum <num>] [-name <name>] [-size [-=+]<size>] [-nlinks <num>] [-exec <path>]\n";
    exit(EXIT_FAILURE);
}

void error_occured(string message) {
    cout << message << "\nCause " << strerror(errno) << '\n';
}

struct modifiers {
private:
    ino_t inode;
    off_t size;
    nlink_t links;
    string path;
    string name;

    bool inode_present = false;
    char size_present = 0;
    bool links_present = false;
    bool path_present = false;
    bool name_present = false;

public:
    void set_inode(string val) {
        unsigned long long num;
        try {
            num = stoull(val);
        } catch (invalid_argument &) {
            wrong_usage("Invalid argument " + val + " for \'-inode\' modifier");
        } catch (out_of_range &) {
            wrong_usage("Argument " + val + " out of range for \'-inode\' modifier");
        }
        if (numeric_limits<ino_t>::min() > num || numeric_limits<ino_t>::max() < num) {
            wrong_usage("Argument " + val + " out of range for \'-inode\' modifier");
        }
        inode = num;
        inode_present = true;
    }

    void set_name(string val) {
        name = val;
        name_present = true;
    }

    void set_size(string val) {
        if (val[0] == '-') {
            size_present = 1;
            val = val.substr(1);
        } else if (val[0] == '+') {
            size_present = 2;
            val = val.substr(1);
        } else if (val[0] == '=') {
            size_present = 3;
            val = val.substr(1);
        } else {
            size_present = 3;
        }
        long long num;
        try {
            num = stoll(val);
        } catch (invalid_argument &) {
            wrong_usage("Invalid argument " + val + " for \'-size\' modifier");
        } catch (out_of_range &) {
            wrong_usage("Argument " + val + " out of range for \'-size\' modifier");
        }
        if (numeric_limits<off_t>::min() > num || numeric_limits<off_t>::max() < num) {
            wrong_usage("Argument " + val + " out of range for \'-size\' modifier");
        }
        size = num;
    }

    void set_links(string val) {
        unsigned long long num;
        try {
            num = stoull(val);
        } catch (invalid_argument &) {
            wrong_usage("Invalid argument " + val + " for \'-nlinks\' modifier");
        } catch (out_of_range &) {
            wrong_usage("Argument " + val + " out of range for \'-nlinks\' modifier");
        }
        if (numeric_limits<nlink_t>::min() > num || numeric_limits<nlink_t>::max() < num) {
            wrong_usage("Argument " + val + " out of range for \'-nlinks\' modifier");
        }
        links = num;
        links_present = true;
    }

    void set_path(string val) {
        path = val;
        path_present = true;
    }

    bool accepts(string name, string path) {
        struct stat buf;
        if (stat(path.c_str(), &buf) < 0) {
            error_occured("Error getting information about file " + path);
            return false;
        }
        return (!inode_present || buf.st_ino == inode) &&
               (!links_present || buf.st_nlink == links) &&
               (!size_present || (size_present == 1 && buf.st_size < size) ||
               (size_present == 2 && buf.st_size > size) || (size_present && buf.st_size == size)) &&
               (!name_present || name == this->name);
    }

    bool is_exec() {
        return path_present;
    }

    string get_path() {
        return path;
    }
};

void walk(string dir_str, modifiers &mods, vector<string> &files) {
    DIR *dir = opendir(dir_str.c_str());
    if (dir == NULL) {
        error_occured("Cannot open dir " + dir_str);
        return;
    }
    dirent *entry;
    errno = 0;
    while (entry = readdir(dir)) {
        string name = entry->d_name;
        string path = dir_str + "/" + name;
        if (entry->d_type == DT_DIR) {
            if (name == "." || name == "..")
                continue;
            walk(path, mods, files);
        } else if (entry->d_type == DT_REG) {
            if (mods.accepts(name, path)) {
                files.push_back(path);
            }
        }
        errno = 0;
    }
    if (errno != 0) {
        error_occured("Error reading directory " + dir_str);
    }
    if (closedir(dir) < 0) {
        error_occured("Error closing directory " + dir_str);
    }
}

void execute(vector<char *> &task) {
    int st;
    pid_t child_pid = fork();
    if (child_pid < 0) {
        error_occured("Cannot fork");
    } else if (child_pid == 0) {
        if (execvp(task[0], task.data()) < 0) {
            error_occured("Cannot execute task " + string(task[0]));
            exit(EXIT_FAILURE);
        }
    } else {
        if (waitpid(child_pid, &st , WUNTRACED) < 0) {
            error_occured("Error while executing task " + string(task[0]));
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2 || (argc % 2) != 0) {
        wrong_usage("Wrong number of arguments");
    }
    string dir = argv[1];
    modifiers mods;
    for (int i = 2; i < argc; i+= 2) {
        string mod = argv[i];
        string val = argv[i + 1];
        if (mod == "-inum") {
            mods.set_inode(val);
        } else if (mod == "-name") {
            mods.set_name(val);
        } else if (mod == "-size") {
            mods.set_size(val);
        } else if (mod == "-nlinks") {
            mods.set_links(val);
        } else if (mod == "-exec") {
            mods.set_path(val);
        } else {
            wrong_usage("No such modifier \'" + mod + "\'");
        }
    }
    vector<string> files;
    walk(dir, mods, files);
    cout << "Found " << files.size() << " files\n";
    for (auto &s: files) {
        cout << s << '\n';
    }
    if (mods.is_exec()) {
        vector<char *> task(3);
        task[0] = const_cast<char *>(mods.get_path().c_str());
        task[2] = NULL;
        for (auto &file: files) {
            task[1] = const_cast<char *>(file.c_str());
            execute(task);
        }
    }
    return 0;
}
