#include <iostream>
#include <string>

#ifdef USE_GNU_READLINE
#include <readline/readline.h>
#include <readline/history.h>

inline bool readline_init() {
    rl_initialize();
    rl_bind_keyseq("\\e", rl_vi_editing_mode);
    return true;
}

inline void readline_deinit() {
    rl_clear_history();
}

inline bool nextline(const char *prompt, std::string &line) {
    if (char *input = readline(prompt); input) {
        line.assign(input);
        free(input);
        return true;
    }

    return false;
}

inline void add_history(const std::string &line) {
    add_history(line.data());
}

#else // NO GNU READLINE

inline bool readline_init() { return true; }
inline void readline_deinit() { }

inline bool nextline(const char *prompt, std::string &line) {
    std::cout << prompt;
    std::cout.flush();
    return static_cast<bool>(std::getline(std::cin, line));
}

inline void add_history(const std::string &line) { }

#endif
