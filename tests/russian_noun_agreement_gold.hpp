#pragma once

#include <array>
#include <cstddef>

// Product-authored test gold from the approved 14-noun scope. This is deliberately
// independent of the production catalog, formatters, and historical observations.
namespace noun_agreement_gold {

enum class Gender : std::size_t { Masculine, Feminine, Neuter };

struct Noun {
    const char* id;
    std::array<const char*, 3> forms;
    Gender gender;
};

inline constexpr Noun nouns[] = {
    {"book", {"книга", "книги", "книг"}, Gender::Feminine},
    {"error", {"ошибка", "ошибки", "ошибок"}, Gender::Feminine},
    {"task", {"задача", "задачи", "задач"}, Gender::Feminine},
    {"car", {"машина", "машины", "машин"}, Gender::Feminine},
    {"team", {"команда", "команды", "команд"}, Gender::Feminine},
    {"line", {"строка", "строки", "строк"}, Gender::Feminine},
    {"table", {"стол", "стола", "столов"}, Gender::Masculine},
    {"house", {"дом", "дома", "домов"}, Gender::Masculine},
    {"file", {"файл", "файла", "файлов"}, Gender::Masculine},
    {"server", {"сервер", "сервера", "серверов"}, Gender::Masculine},
    {"window", {"окно", "окна", "окон"}, Gender::Neuter},
    {"word", {"слово", "слова", "слов"}, Gender::Neuter},
    {"place", {"место", "места", "мест"}, Gender::Neuter},
    {"message", {"сообщение", "сообщения", "сообщений"}, Gender::Neuter},
};

struct Number {
    const char* digits;
    std::size_t form;
    // Masculine, feminine, neuter. Spellings and noun-form slots are literal
    // reviewed gold, not computed with the implementation's category formula.
    std::array<const char*, 3> words;
};

inline constexpr Number numbers[] = {
    {"0", 2, {"ноль", "ноль", "ноль"}},
    {"1", 0, {"один", "одна", "одно"}},
    {"2", 1, {"два", "две", "два"}},
    {"3", 1, {"три", "три", "три"}},
    {"4", 1, {"четыре", "четыре", "четыре"}},
    {"5", 2, {"пять", "пять", "пять"}},
    {"11", 2, {"одиннадцать", "одиннадцать", "одиннадцать"}},
    {"21", 0, {"двадцать один", "двадцать одна", "двадцать одно"}},
    {"22", 1, {"двадцать два", "двадцать две", "двадцать два"}},
    {"25", 2, {"двадцать пять", "двадцать пять", "двадцать пять"}},
    {"101", 0, {"сто один", "сто одна", "сто одно"}},
    {"102", 1, {"сто два", "сто две", "сто два"}},
    {"111", 2, {"сто одиннадцать", "сто одиннадцать", "сто одиннадцать"}},
    {"112", 2, {"сто двенадцать", "сто двенадцать", "сто двенадцать"}},
    {"113", 2, {"сто тринадцать", "сто тринадцать", "сто тринадцать"}},
    {"114", 2, {"сто четырнадцать", "сто четырнадцать", "сто четырнадцать"}},
    {"121", 0, {"сто двадцать один", "сто двадцать одна", "сто двадцать одно"}},
    {"122", 1, {"сто двадцать два", "сто двадцать две", "сто двадцать два"}},
};

struct ChangedInventoryRow {
    const char* id;
    const char* expected;
};

inline constexpr ChangedInventoryRow changed_inventory_rows[] = {
    {"feminine-task-01", "одна задача"},
    {"feminine-task-02", "две задачи"},
    {"feminine-task-21", "двадцать одна задача"},
    {"feminine-task-22", "двадцать две задачи"},
    {"feminine-car-01", "одна машина"},
    {"feminine-car-02", "две машины"},
    {"feminine-car-21", "двадцать одна машина"},
    {"feminine-car-22", "двадцать две машины"},
    {"feminine-team-01", "одна команда"},
    {"feminine-team-02", "две команды"},
    {"feminine-team-21", "двадцать одна команда"},
    {"feminine-team-22", "двадцать две команды"},
    {"feminine-line-01", "одна строка"},
    {"feminine-line-02", "две строки"},
    {"feminine-line-21", "двадцать одна строка"},
    {"feminine-line-22", "двадцать две строки"},
    {"neuter-window-01", "одно окно"},
    {"neuter-window-21", "двадцать одно окно"},
    {"neuter-word-01", "одно слово"},
    {"neuter-word-21", "двадцать одно слово"},
    {"neuter-place-01", "одно место"},
    {"neuter-place-21", "двадцать одно место"},
    {"neuter-message-01", "одно сообщение"},
    {"neuter-message-21", "двадцать одно сообщение"},
};

struct ChangedControl {
    const char* id;
    const char* normalized;
    const char* pronunciation;
};

inline constexpr ChangedControl changed_controls[] = {
    {"task-comma", "две задачи, затем три.", "две задачи, затем три."},
    {"task-newline", "две задачи три", "две задачи три"},
    {"task-independent", "две задачи четыре", "две задачи четыре"},
    {"task-minus", "минус две задачи", "минус две задачи"},
    {"email", "user2@example.com и две задачи", "user2@example.com и две задачи"},
    {"rtx", "RTX 4090 и две задачи", "эр ти икс сорок девяносто и две задачи"},
    {"mixed-head", "OpenAI две задачи", "OpenAI две задачи"},
    {"units", "два килограмма и две задачи", "два килограмма и две задачи"},
    {"currency", "два рубля и две задачи", "два рубля и две задачи"},
    {"time", "один час две минуты и две задачи", "один час две минуты и две задачи"},
    {"date",
     "первое февраля две тысячи двадцать шестого года и две задачи",
     "первое февраля две тысячи двадцать шестого года и две задачи"},
};

} // namespace noun_agreement_gold
