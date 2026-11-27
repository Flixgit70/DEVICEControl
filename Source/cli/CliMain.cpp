#include <JuceHeader.h>
#include "CliTransport.h"
#include <iostream>
#include <vector>

#if JUCE_WINDOWS
 #include <conio.h>
 #include <windows.h>
#else
 #include <sys/select.h>
 #include <termios.h>
 #include <unistd.h>
#endif

namespace
{
constexpr auto prompt = "DEVICEControl> ";
constexpr int keyUp = -1001;
constexpr int keyDown = -1002;
constexpr int keyLeft = -1003;
constexpr int keyRight = -1004;
constexpr int keyHome = -1005;
constexpr int keyEnd = -1006;
constexpr int keyDelete = -1007;
constexpr int keyIgnored = -1008;

bool isShowVersionCommand(const juce::String& command)
{
    juce::StringArray words;
    words.addTokens(command.trim(), " \t", "\"'");
    words.removeEmptyStrings();
    return words.size() == 2
        && juce::String("show").startsWithIgnoreCase(words[0])
        && juce::String("version").startsWithIgnoreCase(words[1]);
}

er::CliClient::Response requestCommand(er::CliClient& client, const juce::String& command,
                                       er::CliCommandModel::Mode mode)
{
    auto response = client.request(command, mode);
    if (mode == er::CliCommandModel::Mode::execute && isShowVersionCommand(command))
        response.text = "DEVICEControlCLI " JUCE_APPLICATION_VERSION_STRING "\n" + response.text;
    return response;
}

void redraw(const juce::String& line, int cursorPosition)
{
    std::cout << "\r\033[2K" << prompt << line;
    const auto charactersAfterCursor = line.length() - cursorPosition;
    if (charactersAfterCursor > 0)
        std::cout << "\033[" << charactersAfterCursor << "D";
    std::cout << std::flush;
}

int readCharacter()
{
#if JUCE_WINDOWS
    const auto character = _getwch();
    if (character == 0 || character == 0xe0)
    {
        const auto extended = _getwch();
        if (extended == 72) return keyUp;
        if (extended == 80) return keyDown;
        if (extended == 75) return keyLeft;
        if (extended == 77) return keyRight;
        if (extended == 71) return keyHome;
        if (extended == 79) return keyEnd;
        if (extended == 83) return keyDelete;
        return keyIgnored;
    }
    return character;
#else
    unsigned char character = 0;
    if (::read(STDIN_FILENO, &character, 1) != 1) return -1;
    if (character != 27) return character;

    unsigned char sequence[2] {};
    if (::read(STDIN_FILENO, &sequence[0], 1) != 1
        || ::read(STDIN_FILENO, &sequence[1], 1) != 1)
        return keyIgnored;
    if (sequence[0] == '[' && sequence[1] == 'A') return keyUp;
    if (sequence[0] == '[' && sequence[1] == 'B') return keyDown;
    if (sequence[0] == '[' && sequence[1] == 'C') return keyRight;
    if (sequence[0] == '[' && sequence[1] == 'D') return keyLeft;
    if ((sequence[0] == '[' || sequence[0] == 'O') && sequence[1] == 'H') return keyHome;
    if ((sequence[0] == '[' || sequence[0] == 'O') && sequence[1] == 'F') return keyEnd;
    if (sequence[0] == '[' && sequence[1] == '3')
    {
        unsigned char suffix = 0;
        if (::read(STDIN_FILENO, &suffix, 1) == 1 && suffix == '~') return keyDelete;
    }
    return keyIgnored;
#endif
}

bool insertInputCharacter(int firstCharacter, juce::String& line, int& cursorPosition)
{
    juce::String inserted;
#if JUCE_WINDOWS
    if (firstCharacter < 32 || firstCharacter == 127) return false;
    inserted = juce::String::charToString((juce::juce_wchar) firstCharacter);
#else
    if (firstCharacter < 32 || firstCharacter == 127) return false;
    char utf8[5] { (char) firstCharacter, 0, 0, 0, 0 };
    int byteCount = 1;
    if ((firstCharacter & 0xe0) == 0xc0) byteCount = 2;
    else if ((firstCharacter & 0xf0) == 0xe0) byteCount = 3;
    else if ((firstCharacter & 0xf8) == 0xf0) byteCount = 4;
    for (int index = 1; index < byteCount; ++index)
    {
        const auto next = readCharacter();
        if (next < 0 || (next & 0xc0) != 0x80) return false;
        utf8[index] = (char) next;
    }
    inserted = juce::String::fromUTF8(utf8, byteCount);
#endif
    line = line.substring(0, cursorPosition) + inserted + line.substring(cursorPosition);
    cursorPosition += inserted.length();
    return true;
}

class RawTerminal
{
public:
    RawTerminal()
    {
#if JUCE_WINDOWS
        inputHandle = GetStdHandle(STD_INPUT_HANDLE);
        active = inputHandle != INVALID_HANDLE_VALUE
              && GetConsoleMode(inputHandle, &originalMode) != 0;
        if (active)
            SetConsoleMode(inputHandle, originalMode & ~(ENABLE_LINE_INPUT
                                                        | ENABLE_ECHO_INPUT
                                                        | ENABLE_PROCESSED_INPUT));
#else
        active = ::isatty(STDIN_FILENO) != 0 && ::tcgetattr(STDIN_FILENO, &original) == 0;
        if (active)
        {
            auto raw = original;
            raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO | ISIG);
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        }
#endif
    }

    ~RawTerminal()
    {
#if JUCE_WINDOWS
        if (active) SetConsoleMode(inputHandle, originalMode);
#else
        if (active) ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
#endif
    }

private:
#if JUCE_WINDOWS
    HANDLE inputHandle = INVALID_HANDLE_VALUE;
    DWORD originalMode = 0;
    bool active = false;
#else
    termios original {};
    bool active = false;
#endif
};

bool interactiveLine(er::CliClient& client, juce::String& line,
                     const std::vector<juce::String>& history)
{
    line.clear();
    int cursorPosition = 0;
    auto historyIndex = history.size();
    juce::String draft;
    redraw(line, cursorPosition);
    while (true)
    {
        const auto character = readCharacter();
        if (character == -1 || character == 3 || (character == 4 && line.isEmpty()))
        {
            std::cout << "\n";
            return false;
        }
        if (character == '\r' || character == '\n')
        {
            std::cout << "\n";
            return true;
        }
        if (character == 8 || character == 127)
        {
            if (cursorPosition > 0)
            {
                line = line.substring(0, cursorPosition - 1) + line.substring(cursorPosition);
                --cursorPosition;
            }
            redraw(line, cursorPosition);
            continue;
        }
        if (character == keyUp)
        {
            if (!history.empty() && historyIndex > 0)
            {
                if (historyIndex == history.size()) draft = line;
                line = history[--historyIndex];
                cursorPosition = line.length();
            }
            redraw(line, cursorPosition);
            continue;
        }
        if (character == keyDown)
        {
            if (historyIndex < history.size())
            {
                ++historyIndex;
                line = historyIndex < history.size() ? history[historyIndex] : draft;
                cursorPosition = line.length();
            }
            redraw(line, cursorPosition);
            continue;
        }
        if (character == keyLeft)
        {
            if (cursorPosition > 0) --cursorPosition;
            redraw(line, cursorPosition);
            continue;
        }
        if (character == keyRight)
        {
            if (cursorPosition < line.length()) ++cursorPosition;
            redraw(line, cursorPosition);
            continue;
        }
        if (character == keyHome)
        {
            cursorPosition = 0;
            redraw(line, cursorPosition);
            continue;
        }
        if (character == keyEnd)
        {
            cursorPosition = line.length();
            redraw(line, cursorPosition);
            continue;
        }
        if (character == keyDelete || (character == 4 && line.isNotEmpty()))
        {
            if (cursorPosition < line.length())
                line = line.substring(0, cursorPosition) + line.substring(cursorPosition + 1);
            redraw(line, cursorPosition);
            continue;
        }
        if (character == '\t' || character == '?')
        {
            const auto mode = character == '\t' ? er::CliCommandModel::Mode::complete
                                                 : er::CliCommandModel::Mode::help;
            const auto commandBeforeCursor = line.substring(0, cursorPosition);
            const auto response = requestCommand(client, commandBeforeCursor, mode);
            if (response.completedLine.isNotEmpty())
            {
                line = response.completedLine + line.substring(cursorPosition);
                cursorPosition = response.completedLine.length();
            }
            if (response.text.isNotEmpty() && (response.completedLine.isEmpty() || response.candidates.size() > 1))
                std::cout << "\n" << response.text << "\n";
            redraw(line, cursorPosition);
            continue;
        }
        if (insertInputCharacter(character, line, cursorPosition))
        {
            redraw(line, cursorPosition);
        }
    }
}

bool waitForWatchStop(int milliseconds)
{
    for (int elapsed = 0; elapsed < milliseconds; elapsed += 50)
    {
#if JUCE_WINDOWS
        if (_kbhit())
        {
            const auto character = _getwch();
            if (character == 3) return true;
            if (character == 0 || character == 0xe0) (void) _getwch();
        }
#else
        fd_set input;
        FD_ZERO(&input);
        FD_SET(STDIN_FILENO, &input);
        timeval timeout { 0, 0 };
        if (::select(STDIN_FILENO + 1, &input, nullptr, nullptr, &timeout) > 0)
        {
            unsigned char character = 0;
            if (::read(STDIN_FILENO, &character, 1) == 1 && character == 3) return true;
        }
#endif
        juce::Thread::sleep(50);
    }
    return false;
}

bool extractWatchCommand(const juce::String& command, juce::String& watchedCommand,
                         bool& markChanges)
{
    const auto pipe = command.lastIndexOfChar('|');
    if (pipe < 0) return false;
    const auto watchOptions = command.substring(pipe + 1).trim();
    if (watchOptions.equalsIgnoreCase("watch")) markChanges = false;
    else if (watchOptions.equalsIgnoreCase("watch changes")
             || watchOptions.equalsIgnoreCase("watch -d")
             || watchOptions.equalsIgnoreCase("watch --differences"))
        markChanges = true;
    else
        return false;
    watchedCommand = command.substring(0, pipe).trim();
    return true;
}

juce::String markChangedLines(const juce::String& previous, const juce::String& current)
{
    if (previous.isEmpty()) return current;

    juce::StringArray previousLines;
    juce::StringArray currentLines;
    previousLines.addLines(previous);
    currentLines.addLines(current);
    juce::String result;
    for (int index = 0; index < currentLines.size(); ++index)
    {
        const auto changed = index >= previousLines.size()
                          || currentLines[index] != previousLines[index];
        if (changed)
            result << "\033[1;35m>> " << currentLines[index] << "\033[0m";
        else
            result << "   " << currentLines[index];
        if (index + 1 < currentLines.size()) result << "\n";
    }
    return result;
}

void runWatch(er::CliClient& client, const juce::String& command, bool markChanges)
{
    juce::String previousOutput;
    while (true)
    {
        const auto response = requestCommand(client, command, er::CliCommandModel::Mode::execute);
        const auto displayedOutput = markChanges ? markChangedLines(previousOutput, response.text)
                                                 : response.text;
        std::cout << "\033[2J\033[H"
                  << "Every 5.0s: " << command << "    "
                  << juce::Time::getCurrentTime().toString(true, true, true, true) << "\n"
                  << (markChanges ? "Changes: >> highlighted in magenta.  " : "")
                  << "Press Ctrl+C to stop watching.\n\n"
                  << displayedOutput << "\n" << std::flush;
        previousOutput = response.text;
        if (!response.transportOk || waitForWatchStop(5000)) break;
    }
    std::cout << "\n";
}
}

int main(int argc, char* argv[])
{
    er::CliClient client;

    if (argc == 2 && (juce::String(argv[1]) == "--version" || juce::String(argv[1]) == "-V"))
    {
        std::cout << "DEVICEControlCLI " JUCE_APPLICATION_VERSION_STRING "\n";
        return 0;
    }

    if (argc > 1)
    {
        if (argc == 2)
        {
            const auto response = requestCommand(client, juce::String::fromUTF8(argv[1]),
                                                 er::CliCommandModel::Mode::execute);
            std::cout << response.text << "\n";
            return response.transportOk && response.commandOk ? 0 : 1;
        }

        juce::StringArray arguments;
        for (int i = 1; i < argc; ++i)
        {
            auto argument = juce::String::fromUTF8(argv[i]);
            if (argument.containsAnyOf(" \t\"'"))
                argument = "\"" + argument.replace("\"", "\\\"") + "\"";
            arguments.add(argument);
        }
        const auto response = requestCommand(client, arguments.joinIntoString(" "),
                                             er::CliCommandModel::Mode::execute);
        std::cout << response.text << "\n";
        return response.transportOk && response.commandOk ? 0 : 1;
    }

    const auto connected = requestCommand(client, "show status", er::CliCommandModel::Mode::execute);
    if (!connected.transportOk)
    {
        std::cerr << connected.text << "\n";
        return 1;
    }

    std::cout << "DEVICEControlCLI " JUCE_APPLICATION_VERSION_STRING "\n"
                 "DEVICEControl read-only CLI\n"
                 "Connected to the running DEVICEControl application.\n"
                 "Type show ? or press ? for context help.\n"
                 "Use arrows to recall and edit history; append | watch or | watch changes for refresh.\n"
                 "Type exit to close the CLI.\n\n";

    RawTerminal terminal;
    juce::String line;
    std::vector<juce::String> history;
    while (interactiveLine(client, line, history))
    {
        const auto command = line.trim();
        if (command.isEmpty()) continue;
        if (history.empty() || history.back() != command) history.push_back(command);
        if (history.size() > 100) history.erase(history.begin());
        if (command.equalsIgnoreCase("exit") || command.equalsIgnoreCase("quit")) break;

        juce::String watchedCommand;
        bool markChanges = false;
        if (extractWatchCommand(command, watchedCommand, markChanges))
        {
            if (watchedCommand.isEmpty()) std::cout << "% Missing command before | watch\n";
            else runWatch(client, watchedCommand, markChanges);
            continue;
        }
        const auto response = requestCommand(client, command, er::CliCommandModel::Mode::execute);
        std::cout << response.text << "\n";
    }
    return 0;
}
