#define _WIN32_WINNT 0x0A00
#include <windows.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr const char* kConfigPath = "config.txt";
constexpr const char* kMacroPath = "macros.txt";

enum class ActionType {
    Move,
    Click,
    Key,
    Delay
};

struct Action {
    ActionType type;
    int a = 0;
    int b = 0;
};

struct Macro {
    std::string name;
    std::vector<Action> actions;
};

struct AppConfig {
    int default_delay_ms = 50;
};

AppConfig LoadConfig() {
    AppConfig config;
    std::ifstream in(kConfigPath);
    if (!in) {
        return config;
    }
    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        if (key == "default_delay_ms") {
            try {
                config.default_delay_ms = std::stoi(value);
            } catch (const std::exception&) {
                config.default_delay_ms = 50;
            }
        }
    }
    return config;
}

void SaveConfig(const AppConfig& config) {
    std::ofstream out(kConfigPath, std::ios::trunc);
    if (!out) {
        std::cerr << "Failed to write config file." << std::endl;
        return;
    }
    out << "default_delay_ms=" << config.default_delay_ms << "\n";
}

std::vector<Macro> LoadMacros() {
    std::vector<Macro> macros;
    std::ifstream in(kMacroPath);
    if (!in) {
        return macros;
    }
    std::string line;
    Macro current;
    bool in_macro = false;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        if (token == "macro") {
            if (in_macro) {
                macros.push_back(current);
                current = Macro{};
            }
            in_macro = true;
            std::getline(iss, current.name);
            if (!current.name.empty() && current.name.front() == ' ') {
                current.name.erase(0, 1);
            }
        } else if (token == "end") {
            if (in_macro) {
                macros.push_back(current);
                current = Macro{};
                in_macro = false;
            }
        } else if (in_macro) {
            if (token == "move") {
                int dx = 0;
                int dy = 0;
                if (iss >> dx >> dy) {
                    current.actions.push_back({ActionType::Move, dx, dy});
                }
            } else if (token == "click") {
                std::string button;
                if (iss >> button) {
                    int code = (button == "right") ? 1 : 0;
                    current.actions.push_back({ActionType::Click, code, 0});
                }
            } else if (token == "key") {
                int vk = 0;
                if (iss >> vk) {
                    current.actions.push_back({ActionType::Key, vk, 0});
                }
            } else if (token == "delay") {
                int ms = 0;
                if (iss >> ms) {
                    current.actions.push_back({ActionType::Delay, ms, 0});
                }
            }
        }
    }
    if (in_macro) {
        macros.push_back(current);
    }
    return macros;
}

void SaveMacros(const std::vector<Macro>& macros) {
    std::ofstream out(kMacroPath, std::ios::trunc);
    if (!out) {
        std::cerr << "Failed to write macros file." << std::endl;
        return;
    }
    for (const auto& macro : macros) {
        out << "macro " << macro.name << "\n";
        for (const auto& action : macro.actions) {
            switch (action.type) {
            case ActionType::Move:
                out << "move " << action.a << " " << action.b << "\n";
                break;
            case ActionType::Click:
                out << "click " << (action.a == 1 ? "right" : "left") << "\n";
                break;
            case ActionType::Key:
                out << "key " << action.a << "\n";
                break;
            case ActionType::Delay:
                out << "delay " << action.a << "\n";
                break;
            }
        }
        out << "end\n";
    }
}

void PrintMenu() {
    std::cout << "\n=== Macro Console (Driver-Free) ===\n";
    std::cout << "1) List macros\n";
    std::cout << "2) Run macro\n";
    std::cout << "3) Create macro\n";
    std::cout << "4) Delete macro\n";
    std::cout << "5) Set default delay\n";
    std::cout << "6) Save\n";
    std::cout << "7) Load\n";
    std::cout << "0) Quit\n";
}

void ListMacros(const std::vector<Macro>& macros) {
    if (macros.empty()) {
        std::cout << "No macros available.\n";
        return;
    }
    for (size_t i = 0; i < macros.size(); ++i) {
        std::cout << (i + 1) << ") " << macros[i].name << " (" << macros[i].actions.size() << " steps)\n";
    }
}

void SendMouseMove(int dx, int dy) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}

void SendMouseClick(bool right) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[1].type = INPUT_MOUSE;
    if (right) {
        inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    } else {
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    }
    SendInput(2, inputs, sizeof(INPUT));
}

void SendKey(int vk) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = static_cast<WORD>(vk);
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = static_cast<WORD>(vk);
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void RunMacro(const Macro& macro, const AppConfig& config) {
    std::cout << "Running macro: " << macro.name << "\n";
    for (const auto& action : macro.actions) {
        switch (action.type) {
        case ActionType::Move:
            SendMouseMove(action.a, action.b);
            Sleep(config.default_delay_ms);
            break;
        case ActionType::Click:
            SendMouseClick(action.a == 1);
            Sleep(config.default_delay_ms);
            break;
        case ActionType::Key:
            SendKey(action.a);
            Sleep(config.default_delay_ms);
            break;
        case ActionType::Delay:
            Sleep(action.a);
            break;
        }
    }
    std::cout << "Done.\n";
}

int ReadIndex(const std::string& prompt) {
    std::cout << prompt;
    std::string input;
    if (!std::getline(std::cin, input)) {
        return -1;
    }
    try {
        return std::stoi(input);
    } catch (const std::exception&) {
        return -1;
    }
}

std::string ReadLine(const std::string& prompt) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void CreateMacro(std::vector<Macro>& macros) {
    Macro macro;
    macro.name = ReadLine("Macro name: ");
    if (macro.name.empty()) {
        std::cout << "Name cannot be empty.\n";
        return;
    }

    while (true) {
        std::cout << "Add step (move/click/key/delay/done): ";
        std::string step;
        if (!std::getline(std::cin, step)) {
            break;
        }
        if (step == "done") {
            break;
        }
        if (step == "move") {
            int dx = ReadIndex("dx: ");
            int dy = ReadIndex("dy: ");
            if (dx == -1 || dy == -1) {
                std::cout << "Invalid numbers.\n";
                continue;
            }
            macro.actions.push_back({ActionType::Move, dx, dy});
        } else if (step == "click") {
            std::string button = ReadLine("left/right: ");
            int code = (button == "right") ? 1 : 0;
            macro.actions.push_back({ActionType::Click, code, 0});
        } else if (step == "key") {
            int vk = ReadIndex("Virtual-Key code (e.g. 0x41 for A): ");
            if (vk == -1) {
                std::cout << "Invalid key code.\n";
                continue;
            }
            macro.actions.push_back({ActionType::Key, vk, 0});
        } else if (step == "delay") {
            int ms = ReadIndex("Delay ms: ");
            if (ms == -1) {
                std::cout << "Invalid delay.\n";
                continue;
            }
            macro.actions.push_back({ActionType::Delay, ms, 0});
        } else {
            std::cout << "Unknown step.\n";
        }
    }

    macros.push_back(macro);
    std::cout << "Macro added.\n";
}
} // namespace

int main() {
    AppConfig config = LoadConfig();
    std::vector<Macro> macros = LoadMacros();

    std::cout << "Driver-free macro console ready (Win11).\n";
    std::cout << "Press Enter to open the menu.\n";
    std::string startup;
    std::getline(std::cin, startup);

    while (true) {
        PrintMenu();
        int choice = ReadIndex("Select option: ");
        if (choice == 0) {
            break;
        }
        if (choice == 1) {
            ListMacros(macros);
        } else if (choice == 2) {
            if (macros.empty()) {
                std::cout << "No macros to run.\n";
                continue;
            }
            ListMacros(macros);
            int index = ReadIndex("Macro number: ");
            if (index < 1 || index > static_cast<int>(macros.size())) {
                std::cout << "Invalid macro number.\n";
                continue;
            }
            RunMacro(macros[index - 1], config);
        } else if (choice == 3) {
            CreateMacro(macros);
        } else if (choice == 4) {
            ListMacros(macros);
            int index = ReadIndex("Macro number to delete: ");
            if (index < 1 || index > static_cast<int>(macros.size())) {
                std::cout << "Invalid macro number.\n";
                continue;
            }
            macros.erase(macros.begin() + (index - 1));
            std::cout << "Macro removed.\n";
        } else if (choice == 5) {
            int delay = ReadIndex("Default delay (ms): ");
            if (delay < 0) {
                std::cout << "Invalid delay.\n";
                continue;
            }
            config.default_delay_ms = delay;
            std::cout << "Default delay set.\n";
        } else if (choice == 6) {
            SaveConfig(config);
            SaveMacros(macros);
            std::cout << "Saved.\n";
        } else if (choice == 7) {
            config = LoadConfig();
            macros = LoadMacros();
            std::cout << "Loaded.\n";
        } else {
            std::cout << "Unknown option.\n";
        }
    }

    SaveConfig(config);
    SaveMacros(macros);
    std::cout << "Goodbye!" << std::endl;
    std::cout << "Press Enter to exit.\n";
    std::string exit_line;
    std::getline(std::cin, exit_line);
    return 0;
}
