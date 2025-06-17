/******************************************************************************
 *  CodeWordle – A Wordle‑style code‑guessing game                            *
 *                                                                            *
 *  Language : C++17                                                          *
 *  GUI      : FLTK 1.4.x(optional)                                           *
 *  Tested on: Windows 10/11(x64)                                             *
 *                                                                            *
 *  Author   : Shao Jiaqi                                                     *
 *                                                                            *
 * ── Build instructions ─────────────────────────────────────────────────────*
 *  ➤ Console‑only version                                                   *
 *      • Comment out the GUI class.                                          *
 *      • Compile: g++ main.cpp -o main -O2 -Wall                             *
 *                                                                            *
 *  ➤ GUI version                                                            *
 *      1. Install FLTK 1.4.3  (https://www.fltk.org).                        *
 *      2. Install MinGW‑w64 and make sure gcc, g++ and make are in PATH.     *
 *      3. In a MinGW terminal:                                               *
 *           cd <FLTK‑source‑dir>                                             *
 *           ./configure                                                      *
 *           mingw32-make                                                     *
 *      4. From your project directory compile with:                          *
 *         g++ main.cpp -o main -O2 -Wall $(fltk-config --cxxflags --ldflags) *
 *                                                                            *
 *  Features                                                                  *
 *      • Three game modes: Limited Guesses, Time Attack, Point.              *
 *      • Optional fuzzy matching and auto‑guess hints.                       *
 *      • Code repository manager and persistent statistics.                  *
 *                                                                            *                                                                            *                                                                            *
 ******************************************************************************/





#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <filesystem>
#include <random>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

class AppException:public std::runtime_error
{
public:
    AppException(const std::string& msg):std::runtime_error("[Error] "+msg){};
};

namespace fs=std::filesystem;

class CodeSnippet
{
private:
    fs::path path;

    std::vector<std::string> code;
    std::vector<std::vector<int> > state;

    bool fuzzyAllowed=true;

    static constexpr int MIN_LEN    =3;
    static constexpr int UNGUESSED  =0;
    static constexpr int FUZZY_MATCH=1;
    static constexpr int EXACT_MATCH=2;
    static constexpr int FUZZY_FAIL =1;
    static constexpr int FUZZY_COUNT=2;

public:
    CodeSnippet(){};

    CodeSnippet(const fs::path& filePath,bool fuzzy=true):path(filePath),fuzzyAllowed(fuzzy)
    {
        loadFromFile();
    }

    void loadFromFile()
    {
        std::ifstream fin(path);
        if(!fin)
            throw AppException("Could not open file: "+path.string());

        // read the file
        std::string line;
        while(std::getline(fin,line))
        {
            // replace '\t' to 4 space
            const std::string tab4(4,' ');
            std::size_t pos=0;
            while((pos=line.find('\t',pos)) != std::string::npos)
            {
                line.replace(pos,1,tab4);
                pos += tab4.size();
            }

            line.append(MIN_LEN-1,' '); // for all non-empty lines can be guessed

            code.push_back(line);
            state.push_back(std::vector<int>((int)line.size()));
        }

        fin.close();
    }

    int getMinLen()
    {
        return MIN_LEN;
    }

    int getTotalNumber()
    {
        // get the number of all visible characters
        int total=0;
        for(auto& line:code)
        {
            for(auto& c:line)
            {
                if(c != ' ')
                    total++;
            }
        }

        return total;
    }

    int getGuessedNumber()
    {
        // get the number of all visible and guessed characters
        int guessed=0;
        for(int i=0;i<(int)code.size();i++)
        {
            auto& line=code[i];
            for(int j=0;j<(int)line.size();j++)
                if(line[j] != ' ' && state[i][j] == EXACT_MATCH)
                    guessed++;
        }

        return guessed;
    }

    void reveal()
    {
        std::vector<std::array<int,2> > posVec;
        for(int i=0;i<(int)code.size();i++)
        {
            auto& line=code[i];
            for(int j=0;j<(int)line.size();j++)
                if(line[j] != ' ' && state[i][j] != EXACT_MATCH)
                    posVec.push_back({i,j});
        }

        if(posVec.empty())
            return;

        // use random number generator to get a random index
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0,(int)posVec.size()-1);

        auto pos=posVec[dist(gen)];

        state[pos[0]][pos[1]]=EXACT_MATCH;
    }

    std::array<int,2> guess(const std::string& guess)
    {
        int len=(int)guess.size();
        if(len<MIN_LEN)
            return {-1,-1}; // -1: unguessed
        
        std::array<int,2> result{};
        if(!fuzzyAllowed)
            result[1]=-1;   // -1: fuzzy match not allowed

        for(int i=0;i<(int)code.size();i++)
        {
            auto& line=code[i];
            for(int j=0;j <= (int)line.size()-len;j++)
            {
                if(line.substr(j,len) == guess)
                {
                    result[0]++;

                    // update the state
                    for(int k=0;k<len;k++)
                        state[i][j+k]=EXACT_MATCH;
                    continue;
                }

                if(!fuzzyAllowed)
                    continue;

                int failmatch=0,count=0;

                // get the failmatch count and the count of guessed characters(>= 2)
                for(int k=0;k<len;k++)
                {
                    if(guess[k] != ' ')
                        count++;

                    if(line[j+k] != guess[k])
                        failmatch++;
                }

                if(failmatch <= FUZZY_FAIL && count >= FUZZY_COUNT)
                {
                    result[1]++;

                    // update the state
                    for(int k=0;k<len;k++)
                        if(state[i][j+k] == UNGUESSED)
                            state[i][j+k]=FUZZY_MATCH;
                }
            }
        }
        return result;
    }

    bool check()
    {
        // check if all characters are guessed
        bool ok=true;
        for(int i=0;i<(int)code.size();i++)
        {
            auto& line=code[i];
            for(int j=0;j<(int)line.size();j++)
                if(line[j] != ' ' && state[i][j] != EXACT_MATCH)
                {
                    ok=false;
                    break;
                }
        }

        return ok;
    }

    std::vector<std::string> getMasked(const char& placeholder='@',const char& fuzzyPlaceholder='#')
    {
        // get the masked code
        std::vector<std::string> result;
        for(int i=0;i<(int)code.size();i++)
        {
            auto line=code[i];
            for(int j=0;j<(int)line.size();j++)
            {
                if(line[j] == ' ' || state[i][j] == EXACT_MATCH)
                    continue;
                if(state[i][j] == UNGUESSED)
                    line[j]=placeholder;
                if(state[i][j] == FUZZY_MATCH)
                    line[j]=fuzzyPlaceholder;
            }
            result.push_back(line);
        }

        return result;
    }
};

class CodeRepo
{
private:
    fs::path root;
    std::vector<fs::path> cacheVec;

    void refresh()
    {
        cacheVec.clear();

        for(auto& e:fs::directory_iterator(root))
            if(e.is_regular_file() && e.path().extension() == ".txt")
                cacheVec.push_back(e.path());

        std::sort(cacheVec.begin(), cacheVec.end(),
                  [](auto& a,auto& b){return a.filename()<b.filename();});
    }

public:
    CodeRepo(){};

    CodeRepo(const fs::path& dir):root(dir)
    {
        if(!fs::exists(root))
            fs::create_directories(root);
        
        refresh();
    };

    fs::path makePath(const std::string& pid)
    {
        return root/(pid+".txt");
    }

    std::vector<std::string> list()
    {
        std::vector<std::string> ids;
        for(auto& id:cacheVec)
            ids.push_back(id.stem().string());
        
        return ids;
    }

    void add(const std::string& pid,const std::vector<std::string> lines)
    {
        auto path=makePath(pid);
        
        std::ofstream fout(path);
        if(!fout)
            throw AppException("Could not open file: "+path.string());
        
        for(auto& line:lines)
            fout << line << '\n';

        refresh();
    }

    bool remove(const std::string& pid)
    {
        auto path=makePath(pid);

        bool ok=fs::remove(path);
        if(ok)
            refresh();
        
        return ok;
    }

    std::string read(const std::string& pid)
    {
        std::ifstream ifs(makePath(pid));
        std::string data((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
        
        return data;
    }

    std::string random()
    {
        if(cacheVec.empty()) 
            return {};

        // use random number generator to get a random index
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0,(int)cacheVec.size()-1);

        auto path=cacheVec[dist(gen)];

        return path.stem().string();
    }

    CodeSnippet loadSnippet(const std::string& pid, bool fuzzy=true)
    {
        return CodeSnippet(makePath(pid),fuzzy);
    }
};

class StatisticsRepo
{
private:
    fs::path path;

    int totalGames       {0};
    int guessLimitedGames{0};
    int timeAttackGames  {0};
    int pointGames       {0};
    int guessLimitedWins {0};
    int timeAttackWins   {0};
    double totalPoints   {0};

    std::vector<std::string> gameHistory;

public:
    StatisticsRepo(){};

    StatisticsRepo(const fs::path& filePath):path(filePath)
    {
        if(!fs::exists(path))
            saveToFile();

        loadFromFile();
    };

    void loadFromFile()
    {
        std::ifstream fin(path,std::ios::binary);
        if(!fin)
            throw AppException("Could not open file: "+path.string());

        fin.read((char*)&totalGames,sizeof(totalGames));
        fin.read((char*)&guessLimitedGames,sizeof(guessLimitedGames));
        fin.read((char*)&timeAttackGames,sizeof(timeAttackGames));
        fin.read((char*)&pointGames,sizeof(pointGames));
        fin.read((char*)&guessLimitedWins,sizeof(guessLimitedWins));
        fin.read((char*)&timeAttackWins,sizeof(timeAttackWins));
        fin.read((char*)&totalPoints,sizeof(totalPoints));

        std::string line;
        while(std::getline(fin,line))
            gameHistory.push_back(line);
        
        fin.close();
    }

    void saveToFile()
    {
        std::ofstream fout(path,std::ios::binary);
        if(!fout)
            throw AppException("Could not open file: "+path.string());
        
        fout.write((char*)&totalGames,sizeof(totalGames));
        fout.write((char*)&guessLimitedGames,sizeof(guessLimitedGames));
        fout.write((char*)&timeAttackGames,sizeof(timeAttackGames));
        fout.write((char*)&pointGames,sizeof(pointGames));
        fout.write((char*)&guessLimitedWins,sizeof(guessLimitedWins));
        fout.write((char*)&timeAttackWins,sizeof(timeAttackWins));
        fout.write((char*)&totalPoints,sizeof(totalPoints));
        
        for(auto& line:gameHistory)
            fout << line << '\n';
        
        fout.close();
    }

    template<typename T>
    void addGame(const std::string& gameHistoryLine,const std::string& gameType,T state)
    {
        if(gameType == "guessLimited")
        {
            guessLimitedGames++;
            guessLimitedWins += state;
            // state is 1(win) or 0(lose)
        }
        else
            if(gameType == "timeAttack")
            {
                timeAttackGames++;
                timeAttackWins += state;
                // state is 1(win) or 0(lose)
            }
            else
                if(gameType == "point")
                {
                    pointGames++;
                    totalPoints += state;
                    // state is points
                }

        totalGames++;
        gameHistory.push_back(gameHistoryLine);
    }

    std::vector<std::string> getStatistics()
    {
        std::vector<std::string> result;

        result.push_back("Total Games: "+std::to_string(totalGames));
        result.push_back("Guess Limited Games: "+std::to_string(guessLimitedWins)
                        +'/'+std::to_string(guessLimitedGames));
        result.push_back("Time Attack Games: "+std::to_string(timeAttackWins)
                        +'/'+std::to_string(timeAttackGames));
        result.push_back("Point Games: "+std::to_string(pointGames));
        result.push_back("Average Points: "+std::to_string(pointGames == 0 ?0:totalPoints/pointGames));
        result.push_back("Total Points: "+std::to_string(totalPoints));

        result.push_back("\n==========Game History==========\n");

        // In time order, so the last game is at the top
        for(int i=(int)gameHistory.size()-1;~i;i--)
            result.push_back(gameHistory[i]);
        
        return result;
    }
};

struct GameHistoryFormatter
{   
    static constexpr int timeWidth    =26;
    static constexpr int gameTypeWidth=18;
    static constexpr int pidWidth     =7;

    static std::string format(const std::string& time,const std::string& gameType,const std::string& pid,const std::string& gameInfo)
    {

        std::ostringstream oss;
        oss << std::left << std::setw(timeWidth) << time
            << std::setw(gameTypeWidth) << gameType
            << std::setw(pidWidth) << pid
            << gameInfo;

        return oss.str();
    }
};

class Game
{
protected:
    CodeRepo repo;
    CodeSnippet snippet;

    StatisticsRepo& stats;

    std::string pid;

    int guesses{0};
    bool fuzzyAllowed=true;
    bool showPID=false;

public:
    Game(const CodeRepo& repo,StatisticsRepo& stats,bool fuzzy,bool show):repo(repo),stats(stats),fuzzyAllowed(fuzzy),showPID(show){}
    
    virtual ~Game(){};

    virtual bool start()
    {
        pid=repo.random();
        if(pid.empty())
            return false;

        snippet=repo.loadSnippet(pid,fuzzyAllowed);

        return true;
    }

    std::string currentId()
    {
        return pid;
    }

    int guessCount()
    {
        return guesses;
    }

    std::vector<std::string> getMasked()
    {
        return snippet.getMasked();
    }

    // get time(for example Tue May 13 17:21:15 2025)
    std::string getTime()
    {
        auto now=std::chrono::system_clock::now();
        auto time=std::chrono::system_clock::to_time_t(now);

        std::string result=std::ctime(&time);

        result.pop_back(); // remove '\n'
        result.push_back(' '); // add space

        return result;
    }

    virtual std::vector<std::string> getGameInfo()
    {
        std::vector<std::string> result;
        result.push_back(std::to_string(guesses)+" guesses\n");

        return result;
    }

    virtual std::vector<std::string> getDisplayLines()
    {
        std::vector<std::string> result=getGameInfo();

        if(showPID)
            result.push_back("Problem: www.luogu.com.cn/problem/"+pid);

        auto temp=getMasked();
        for(auto& line:temp)
            result.push_back(line);

        result.push_back("Enter your guesses(>= 3 chars), or end the game by entering E, or get an auto guess by entering A");

        return result;
    }

    virtual std::vector<std::string> makeGuess(const std::string& guess)
    {
        if(!showPID && guess == "P")
        {
            showPID=true;
            return {"PID showing enabled"};
        }

        if(!fuzzyAllowed && guess == "F")
        {
            fuzzyAllowed=true;
            return {"Fuzzy match enabled"};
        }

        auto result=snippet.guess(guess);
        
        // guess is too short
        if(result[0] == -1)
            return {"Guess must be at least "+std::to_string(snippet.getMinLen())+" chars"};

        ++guesses;

        int count=revealTimes();
        while(count--)
            snippet.reveal();

        std::string msg=std::to_string(result[0])+" matches found";

        // fuzzy match
        if(result[1] != -1)
            msg += ", "+std::to_string(result[1])+" fuzzy matches found";

        msg += '.';

        return {msg};
    }

    virtual std::string Win()
    {
        saveStatistics(true);

        return "You win!";
    }

    virtual std::string Lose()
    {
        saveStatistics(false);

        return "You lose.";
    }

    virtual bool isFinished()
    {
        return snippet.check();
    }

    virtual int revealTimes()=0;
    virtual bool isOver()=0;
    virtual void saveStatistics(bool)=0;
};

class guessLimitedGame:public Game // Guess limited game(for example, 30 guesses)
{
private:
    int maxGuesses;

    static constexpr int revealGuesses=5;

public:
    guessLimitedGame(const CodeRepo& repo,StatisticsRepo& stats,bool fuzzy,bool show):Game(repo,stats,fuzzy,show){};
    
    bool start()
    {
        pid=repo.random();
        if(pid.empty())
            return false;
        
        snippet=repo.loadSnippet(pid,fuzzyAllowed);

        int total=snippet.getTotalNumber();
        maxGuesses=std::max(total/3+5,30); // 30 is the minimum number of guesses

        return true;
    }
    
    std::vector<std::string> getGameInfo()
    {
        std::vector<std::string> result;
        result.push_back("Guesses: "+std::to_string(guesses)+"/"+std::to_string(maxGuesses));

        return result;
    }
    
    int revealTimes()
    {
        return guesses%revealGuesses == 0 ?1:0;
    }

    bool isOver()
    {
        return guesses >= maxGuesses;
    }

    std::string Win()
    {
        saveStatistics(true);

        return "You win! You only used "+std::to_string(guesses)+" guesses!";
    }

    std::string Lose()
    {
        saveStatistics(false);

        return "You lose. You have used "+std::to_string(guesses)+" guesses.";
    }

    void saveStatistics(bool isWin)
    {
        std::string gameType="guessLimited";

        std::string currentTime=getTime();
        std::string gameShowType="Limited Guesses";
        std::string gameInfo="guesses: "+std::to_string(guesses)+"/"+std::to_string(maxGuesses)+
                             " "+(isWin ?"Win":"Lose");

        // format the game history line
        std::string gameHistoryLine=GameHistoryFormatter::format(currentTime,gameShowType,pid,gameInfo);

        stats.addGame(gameHistoryLine,gameType,isWin ?1:0);
    }
};

class timeAttackGame:public Game // Time limited game(for example, 10min)
{
private:
    int maxTime;

    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastRevealTime;

    static constexpr int revealTime=10;

public:
    timeAttackGame(const CodeRepo& repo,StatisticsRepo& stats,bool fuzzy,bool show):Game(repo,stats,fuzzy,show){};
    
    bool start()
    {
        pid=repo.random();
        if(pid.empty())
            return false;
        
        snippet=repo.loadSnippet(pid,fuzzyAllowed);

        int total=snippet.getTotalNumber();
        maxTime=std::max(1.0*total/1.5+10,60.0); // 60 is the minimum time in seconds

        startTime=std::chrono::steady_clock::now();
        lastRevealTime=startTime;

        return true;
    }
    
    std::vector<std::string> getGameInfo()
    {
        std::vector<std::string> result;

        auto now=std::chrono::steady_clock::now();
        auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(now-startTime).count();

        result.push_back("Time: "+std::to_string(elapsed)+"s/"+std::to_string(maxTime)+"s");

        return result;
    }
    
    int revealTimes()
    {
        auto now=std::chrono::steady_clock::now();

        int result=std::chrono::duration_cast<std::chrono::seconds>(now-lastRevealTime).count()/revealTime;
        lastRevealTime += std::chrono::seconds(revealTime*result);

        return result;
    }
    
    bool isOver()
    {
        auto now=std::chrono::steady_clock::now();
        auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(now-startTime).count();

        return elapsed >= maxTime;
    }
    
    std::string Win()
    {
        saveStatistics(true);

        auto now=std::chrono::steady_clock::now();
        auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(now-startTime).count();

        return "You win! You only used "+std::to_string(elapsed)+" seconds!";
    }
    
    std::string Lose()
    {
        saveStatistics(false);

        auto now=std::chrono::steady_clock::now();
        auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(now-startTime).count();

        return "You lose. You have used "+std::to_string(elapsed)+" seconds.";
    }
    
    void saveStatistics(bool isWin)
    {
        std::string gameType="timeAttack";

        auto now=std::chrono::steady_clock::now();
        auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(now-startTime).count();

        std::string currentTime=getTime();
        std::string gameShowType="Time Attack";
        std::string gameInfo="time: "+std::to_string(elapsed)+"s/"+std::to_string(maxTime)+"s "+
                             (isWin ?"Win":"Lose");

        // format the game history line
        std::string gameHistoryLine=GameHistoryFormatter::format(currentTime,gameShowType,pid,gameInfo);

        stats.addGame(gameHistoryLine,gameType,isWin ?1:0);
    }
};

class pointGame:public Game // Point game(caculate points based on guesses)
{
private:
    int totalNumber;
    double points;

    double guessPenalty;
    double pointFactor;
    double rewardFactor;

    static constexpr double showPidPenalty{0.5};
    static constexpr double fuzzyPenalty  {0.8};

public:
    pointGame(const CodeRepo& repo,StatisticsRepo& stats,bool fuzzy,bool show,double penalty=100,double pfactor=500,double rfactor=1.5)
             :Game(repo,stats,fuzzy,show),points(0)
    {
        if(penalty <= 0)
            throw AppException("Penalty must be positive");

        if(pfactor <= 0)
            throw AppException("Point factor must be positive");

        if(rfactor<1.0)
            throw AppException("Reward factor must be not less than 1.0");

        guessPenalty=penalty;
        pointFactor=pfactor;
        rewardFactor=rfactor;
    }
    
    bool start()
    {
        pid=repo.random();
        if(pid.empty())
            return false;
        
        snippet=repo.loadSnippet(pid,fuzzyAllowed);
        totalNumber=snippet.getTotalNumber();

        return true;
    }
    
    void calcPoint()
    {
        int guessed=snippet.getGuessedNumber();

        points=pointFactor*guessed*guessed/totalNumber-
               guessPenalty*guesses;
    
        if(showPID)
            points *= showPidPenalty;
        
        if(fuzzyAllowed)
            points *= fuzzyPenalty;
        
        if(guessed == totalNumber)
            points *= rewardFactor;
    }
    
    std::vector<std::string> getDisplayLines()
    {
        std::vector<std::string> result=getGameInfo();
        
        if(showPID)
            result.push_back("Problem: www.luogu.com.cn/problem/"+pid);
        
        auto temp=snippet.getMasked();
        for(auto& line:temp)
            result.push_back(line);
        
        result.push_back("Enter P to show the problem ID, or F to enable fuzzy match");
        result.push_back("The game will be easier, but you will get LESS points");
        result.push_back("Enter your guesses(>= 3 chars), or end the game by entering E");

        return result;
    }
    
    std::vector<std::string> getGameInfo()
    {
        calcPoint();

        std::vector<std::string> result;
        result.push_back("Points: "+std::to_string(points));

        return result;
    }
    
    int revealTimes()
    {
        // never reveal
        return 0;
    }
    
    bool isOver()
    {
        // you can guess forever
        return false;
    }

    std::string Win()
    {
        saveStatistics(true);

        calcPoint();

        return "You achieved "+std::to_string(points)+" points!";
    }

    std::string Lose()
    {
        // there's no "Lose" in point mode, 
        // even if your points is negative

        return Win();
    }

    void saveStatistics(bool isWin)
    {
        calcPoint();

        std::string gameType="point";

        std::string currentTime=getTime();
        std::string gameShowType="Point";
        std::string gameInfo="points: "+std::to_string(points)+
                               " "+(isWin ?"Win":"Lose");
        
        // format the game history line
        std::string gameHistoryLine=GameHistoryFormatter::format(currentTime,gameShowType,pid,gameInfo);

        stats.addGame(gameHistoryLine,gameType,points);
    }
};

class AutoGuess
{
private:
    const std::string alphabet_="abcdefghijklmnopqrstuvwxyz0123456789_^(){};%=<>+-*&|\"";

    const char placeholder='@';
    const char fuzzyholder='#';

    const int guessLength=3;
    
    const std::vector<std::string> keywords=
    {
        "int","for","if(","els","ret","urn","cla","ass","nam","esp",
        "#in","ude","std","siz","lon","eof","nul","ptr","new","del",
        "ete","whi","ile","con","st ","cou","t<<","cin",">> ","%d ",
        "sca","pri","ntf","<<\"", "\"<<"
    };
    
    int count;

public:
    AutoGuess():count(0){};

    std::string guess(const std::vector<std::string>& mask)
    {
        if(count == (int)keywords.size())
            return random();

        bool visited=false;
        for(auto& line:mask)
        {
            for(int i=0;i <= (int)line.size()-guessLength;i++)
                if(line.substr(i,guessLength) == keywords[count])
                    visited=true;
        }

        count++;

        if(visited)
            return guess(mask);
        
        return keywords[count-1];
    }

    std::string random()
    {
        std::string result;
        
        // use random number generator to get a random index
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0,(int)alphabet_.size()-1);

        for(int i=0;i<guessLength;i++)
            result += alphabet_[dist(gen)];
        
        return result;
    }
};

class UI
{
private:
    fs::path root;
    CodeRepo repo;

    StatisticsRepo stats;

    bool showPID=true;

public:
    UI()
    {
        root=fs::current_path();

        repo=CodeRepo(root/"CodeSnippets");

        stats=StatisticsRepo(root/"Statistics.dat");
    };
    
    void clearScreen()
    {
    #ifdef _WIN32
        // for backward compatibility
        std::cout << "\x1B[2J\x1B[H";
        std::cout << "\x1B[3J";
    #else
        std::cout << "\x1B[2J\x1B[H\x1B[3J";
    #endif

        std::cout.flush();
    }
    
    void pause()
    {
        std::cout << "\n--Enter anything to get back--\n";
        std::cin.get();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
    }
    
    void print(const std::vector<std::string>& lines)
    {
        for(auto& line:lines)
            std::cout << line << '\n';
    }
    
    void mainloop()
    {
        while(true)
        {
            clearScreen();

            showStartPage();

            char op;
            std::cin >> op;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');

            switch(op)
            {
                case 'P':
                    showGamePage();
                    break;
                case 'R':
                    showRulePage();
                    break;
                case 'C':
                    showCodePage();
                    break;
                case 'S':
                    showStatisticsPage();
                    break;
                case 'E':
                    return;
            }
        }
    }
    
    void showStartPage()
    {
        clearScreen();

        std::cout << "Play(P)\n";
        std::cout << "Rule(R)\n";
        std::cout << "Code(C)\n";
        std::cout << "Stats(S)\n";
        std::cout << "Exit(E)\n";
    }
    
    void showRulePage()
    {
        clearScreen();
        
        std::cout << "Code Wordle\n\n";
        std::cout << "You will be given a random code snippet.\n";
        std::cout << "Initially, all characters are hidden.\n";
        std::cout << "All you can see is the shape of the code snippet.\n\n";
        std::cout << "Your goal is to guess out the code snippet.\n";
        std::cout << "To achieve this, you can enter a substring of the code snippet length >= 3.\n";
        std::cout << "Then the matching characters will be revealed.\n\n";
        std::cout << "There's also fuzzy match\n";
        std::cout << "If there's a substring of the code snippet that only differs by 1 character\n";
        std::cout << "The substring will change to fuzzy match characters!\n\n";
        std::cout << "There are 3 game modes:\n";
        std::cout << "Limited Guesses: Use less guesses as possible, to reduce the difficuly, reveal one character every 5 guesses\n";
        std::cout << "Time Attack: Use less time as possible, to reduce the difficuly, reveal one character every 10 seconds\n";
        std::cout << "Point: The score will be calculated based on the guesses. "
                  << "Noteably the fuzzy match and the problem ID showing is disabled initially. "
                  << "You can enable them but the score will be reduced.\n";
        
        pause();
    }
    
    void showCodePage()
    {
        while(true)
        {
            clearScreen();

            std::cout << "Code Repo\n";
            std::cout << "List(L)\n";
            std::cout << "Read(R)\n";
            std::cout << "Add/Edit(A)\n";
            std::cout << "Remove(M)\n";
            std::cout << "Back(B)\n";

            char op;
            std::cin >> op;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
            
            clearScreen();

            switch(op)
            {
                case 'L':
                {
                    auto ids=repo.list();
                    if(ids.empty())
                        std::cout << "No codesnippets\n";
                    
                    print(ids);

                    pause();
                    break;
                }
                case 'R':
                {
                    std::string pid;

                    std::cout << "Enter the code ID to read: ";
                    std::getline(std::cin,pid);
                    
                    auto data=repo.read(pid);
                    if(data.empty())
                        std::cout << "Code not found\n";
                    else
                        std::cout << data << '\n';
                    
                    pause();
                    break;
                }
                case 'A':
                {
                    std::string pid;
                    
                    std::cout << "Enter the code ID: ";
                    std::getline(std::cin,pid);

                    std::cout << "Enter the code, end with entering \"END\"\n";

                    std::vector<std::string> lines;
                    std::string line;

                    while(true)
                    {
                        std::getline(std::cin,line);
                        
                        if(line == "END")
                            break;
                        
                        lines.push_back(line);
                    }

                    repo.add(pid,lines);
                    break;
                }
                case 'M':
                {
                    std::string pid;
                    
                    std::cout << "Enter the code ID: ";
                    std::getline(std::cin,pid);

                    if(!repo.remove(pid))
                        std::cout << "Code not found\n";
                    else
                        std::cout << "Code #" << pid << " removed\n";

                    pause();
                    break;
                }
                case 'B':
                {
                    // back

                    return;
                }
            }
        }
    }
    
    void showStatisticsPage()
    {
        clearScreen();

        auto lines=stats.getStatistics();
        print(lines);

        pause();
    }
    
    void showGamePage()
    {
        clearScreen();

        std::cout << "Game Mode:\n";
        std::cout << "Limited Guesses(G)\n";
        std::cout << "Time Attack(T)\n";
        std::cout << "Point(P)\n";

        char op;
        std::cin >> op;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');

        Game* game=nullptr;

        switch(op)
        {
            case 'G':
                game=new guessLimitedGame(repo,stats,true,true);
                break;
            case 'T':
                game=new timeAttackGame(repo,stats,true,true);
                break;
            case 'P':
                game=new pointGame(repo,stats,false,false);
                break;
            default:
                delete game;
                return;
        }

        if(!game -> start())
        {
            std::cout << "There's no codesnippets\n";
            return;
        }

        std::vector<std::string> msg{};

        AutoGuess ag;

        while(true)
        {
            clearScreen();

            auto lines=game -> getDisplayLines();

            print(lines);
            print(msg);

            if(game -> isOver())
            {
                std::cout << game -> Lose() << '\n';

                pause();
                break;
            }

            if(game -> isFinished())
            {
                std::cout << game -> Win() << '\n';

                pause();
                break;
            }

            std::string guess;
            std::getline(std::cin,guess);

            // exit the game, and get loss
            if(guess == "E")
            {
                std::cout << game -> Lose() << '\n';

                pause();
                break;
            }

            if(guess == "A")
            {
                msg=std::vector{ag.guess(game -> getMasked())};

                continue;
            }

            msg=game -> makeGuess(guess);
        }

        delete game;

        stats.saveToFile();
    }
};

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/Fl_Text_Display.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Box.H> 

class GUI {
private:
    fs::path root;
    CodeRepo repo;

    StatisticsRepo stats;
    Game *game;

    AutoGuess autoGuesser;

    Fl_Window *mainWindow;

    Fl_Window* ruleWindow;

    Fl_Window* statsWindow;
    Fl_Text_Buffer *statsBuffer;

    Fl_Window* codeWindow;
    Fl_Text_Buffer* codeBuffer;

    Fl_Window* addWindow;
    Fl_Input* addIdInput;
    Fl_Multiline_Input* addContentInput;

    Fl_Window* gameWindow;

    Fl_Input *guessInput;
    Fl_Text_Buffer *gameBuffer;

public:
    GUI():game(nullptr), mainWindow(nullptr), ruleWindow(nullptr), 
        statsWindow(nullptr), codeWindow(nullptr), addWindow(nullptr), gameWindow(nullptr)
    {
        root=fs::current_path();

        repo=CodeRepo(root/"CodeSnippets");

        stats=StatisticsRepo(root/"Statistics.dat");

        // main menu
        mainWindow=new Fl_Window(200,240,"Code Wordle");

        Fl_Button *btnPlay =new Fl_Button(50, 20,100,30, "Play");
        Fl_Button *btnRule =new Fl_Button(50, 60,100,30, "Rule");
        Fl_Button *btnCode =new Fl_Button(50,100,100,30, "Code");
        Fl_Button *btnStats=new Fl_Button(50,140,100,30,"Stats");
        Fl_Button *btnExit =new Fl_Button(50,180,100,30, "Exit");

        // set callbacks for main menu buttons
        btnPlay  -> callback(cb_Play, this);
        btnRule  -> callback(cb_Rule, this);
        btnCode  -> callback(cb_Code, this);
        btnStats -> callback(cb_Stats,this);
        btnExit  -> callback(cb_Exit, this);

        mainWindow -> end();
        mainWindow -> show();
    }

    ~GUI()
    {
        if(game)
        {
            delete game;
            game=nullptr;
        }

        // only need to delete Window, when deleting them,
        // the other child controls(like buttons) will be deleted automatically
        if(gameWindow)
            delete gameWindow;

        if(addWindow)
            delete addWindow;

        if(codeWindow)
            delete codeWindow;

        if(statsWindow)
            delete statsWindow;

        if(ruleWindow)
            delete ruleWindow;

        if(mainWindow)
            delete mainWindow;
    }

private:
    // static callback functions
    static void cb_Play(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onPlay();
    }
    
    static void cb_Rule(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onRule();
    }
    
    static void cb_Code(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onCode();
    }
    
    static void cb_Stats(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onStats();
    }
    
    static void cb_Exit(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onExit();
    }
    
    static void cb_RuleBack(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onRuleBack();
    }
    
    static void cb_StatsBack(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onStatsBack();
    }
    
    static void cb_CodeBack(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onCodeBack();
    }
    
    static void cb_List(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onList();
    }
    
    static void cb_Read(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onRead();
    }
    
    static void cb_AddEdit(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onAddEdit();
    }
    
    static void cb_Remove(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onRemove();
    }
    
    static void cb_AddSave(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onAddSave();
    }
    
    static void cb_AddCancel(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onAddCancel();
    }
    
    static void cb_Guess(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onGuess();
    }
    
    static void cb_Auto(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onAuto();
    }
    
    static void cb_GiveUp(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onGiveUp();
    }
    
    static void cb_GameWindowClose(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onGiveUp();
    }
    
    static void cb_CodeWindowClose(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onCodeBack();
    }
    
    static void cb_RuleWindowClose(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onRuleBack();
    }
    
    static void cb_StatsWindowClose(Fl_Widget*, void* userdata)
    {
        GUI *gui=static_cast<GUI*>(userdata);
        gui -> onStatsBack();
    }

    void onPlay()
    {
        // prompt user to select game mode
        int mode=fl_choice("Select Game Mode:","Limited Guesses","Time Attack","Point");
        if(mode<0 || mode>2)
        {
            // dialog closed or canceled
            return;
        }

        // create appropriate game object based on selection
        if(game)
        { 
            delete game; 
            game=nullptr;
        }

        bool fuzzyAllowed=true;
        bool showProblemID=true;
        if(mode == 0)
        {   
            // Limited Guesses
            game=new guessLimitedGame(repo,stats,fuzzyAllowed,showProblemID);
        } 
        else 
            if(mode == 1)
            { 
                // Time Attack
                game=new timeAttackGame(repo,stats,fuzzyAllowed,showProblemID);
            } 
            else 
            { 
                // Point
                fuzzyAllowed=false;
                showProblemID=false;
                game=new pointGame(repo,stats,fuzzyAllowed,showProblemID);
            }

        if(!game->start())
        {
            fl_alert("There's no codesnippets");
            delete game;
            game=nullptr;
            return;
        }

        if(!gameWindow)
        {
            gameWindow=new Fl_Window(800,600,"Game");

            Fl_Text_Display *gameDisplay=new Fl_Text_Display(10,10,780,540);
            
            gameBuffer=new Fl_Text_Buffer();
            
            gameDisplay -> buffer(gameBuffer);
            gameDisplay -> textfont(FL_COURIER); // same character width

            guessInput=new Fl_Input(10,560,400,25);
            
            guessInput -> when(FL_WHEN_ENTER_KEY_ALWAYS); 
            guessInput -> callback(cb_Guess,this);
            
            Fl_Button *btnGuess =new Fl_Button(420,558,80,30,"Guess");
            Fl_Button *btnAuto  =new Fl_Button(510,558,80,30,"Auto");
            Fl_Button *btnGiveUp=new Fl_Button(600,558,80,30,"Give Up");
            
            btnGuess -> callback(cb_Guess,this);
            btnAuto -> callback(cb_Auto,this);
            btnGiveUp -> callback(cb_GiveUp,this);

            // close window via X
            gameWindow -> callback(cb_GameWindowClose,this);
            gameWindow -> end();
        }

        updateGameDisplay(std::vector<std::string>());  // no message initially
        
        mainWindow -> hide(); // hide main menu
        gameWindow -> show();

        // the guess input field
        guessInput -> take_focus();
    }

    void onRule()
    {
        if(!ruleWindow)
        {
            ruleWindow=new Fl_Window(600,400,"Game Rules");
            
            Fl_Text_Display *ruleText=new Fl_Text_Display(10,10,580,340);
            
            Fl_Text_Buffer *ruleBuffer=new Fl_Text_Buffer();

            ruleText -> buffer(ruleBuffer);

            const char* rules_content=
                "Code Wordle\n\n"
                "You will be given a random code snippet.\n"
                "Initially, all characters are hidden.\n"
                "All you can see is the shape of the code snippet.\n\n"
                "Your goal is to guess out the code snippet.\n"
                "To achieve this, you can enter a substring of the code snippet length >= 3.\n"
                "Then the matching characters will be revealed.\n\n"
                "There's also fuzzy match\n"
                "If there's a substring of the code snippet that only differs by 1 character\n"
                "The substring will change to fuzzy match characters!\n\n"
                "There are 3 game modes:\n"
                "Limited Guesses: Use less guesses as possible, to reduce the difficulty, reveal one character every 5 guesses\n"
                "Time Attack: Use less time as possible, to reduce the difficulty, reveal one character every 10 seconds\n"
                "Point: The score will be calculated based on the guesses. Notably, the fuzzy match and the problem ID showing is disabled initially.\n"
                "You can enable them but the score will be reduced.\n";
            
            ruleBuffer -> text(rules_content);

            Fl_Button *btnRuleBack=new Fl_Button(260,360,80,30,"Back");
            btnRuleBack -> callback(cb_RuleBack,this);

            ruleWindow -> callback(cb_RuleWindowClose,this);
            ruleWindow -> end();
        }

        mainWindow -> hide();
        ruleWindow -> show();
    }

    void onCode()
    {
        if(!codeWindow)
        {
            codeWindow=new Fl_Window(800,600,"Code Repository");

            Fl_Button *btnList   =new Fl_Button(130,10,100,30,"List");
            Fl_Button *btnRead   =new Fl_Button(240,10,100,30,"Read");
            Fl_Button *btnAddEdit=new Fl_Button(350,10,100,30,"Add/Edit");
            Fl_Button *btnRemove =new Fl_Button(460,10,100,30,"Remove");
            Fl_Button *btnCodeBack=new Fl_Button(570,10,100,30,"Back");

            btnList -> callback(cb_List,this);
            btnRead -> callback(cb_Read,this);
            btnAddEdit -> callback(cb_AddEdit,this);
            btnRemove -> callback(cb_Remove,this);
            btnCodeBack -> callback(cb_CodeBack,this);
            
            // Output display area
            
            Fl_Text_Display* codeText=new Fl_Text_Display(10,50,780,540);
            
            codeBuffer=new Fl_Text_Buffer();
            
            codeText -> buffer(codeBuffer);
            codeText -> textfont(FL_COURIER);

            codeWindow -> callback(cb_CodeWindowClose,this);
            codeWindow -> end();
        }

        // clear previous output
        codeBuffer -> text("");

        mainWindow -> hide();
        codeWindow -> show();
    }

    void onStats()
    {
        if(!statsWindow)
        {
            statsWindow=new Fl_Window(800,400,"Statistics");
            
            Fl_Text_Display *statsText=new Fl_Text_Display(10,10,780,340);
            
            statsBuffer=new Fl_Text_Buffer();

            statsText -> buffer(statsBuffer);
            statsText -> textfont(FL_COURIER);

            Fl_Button *btnStatsBack=new Fl_Button(360,360,80,30,"Back");

            btnStatsBack -> callback(cb_StatsBack,this);

            statsWindow -> callback(cb_StatsWindowClose,this);
            statsWindow -> end();
        }
        
        std::vector<std::string> lines=stats.getStatistics();
        std::string content;
        for(auto& line:lines)
        {
            if(!line.empty() && line.back() == '\n')
                line.pop_back();

            content += line+"\n";
        }

        statsBuffer -> text(content.c_str());
        
        mainWindow -> hide();
        statsWindow -> show();
    }

    void onExit() 
    {
        // exit the app
        mainWindow -> hide();
    }

    void onRuleBack()
    {
        ruleWindow -> hide();
        mainWindow -> show();
    }

    void onStatsBack()
    {
        statsWindow -> hide();
        mainWindow -> show();
    }

    void onCodeBack()
    {
        codeWindow -> hide();
        mainWindow -> show();
    }

    void onList()
    {
        // list all PIDs
        std::vector<std::string> ids=repo.list();
        if(ids.empty())
        {
            codeBuffer -> text("No codesnippets\n");
            return;
        }

        std::string listStr;
        for(const std::string& pid:ids)
            listStr += pid+"\n";

        codeBuffer -> text(listStr.c_str());
    }

    void onRead()
    {
        const char *ptr=fl_input("Enter the code ID to read:", "");

        // "cancel" button
        if(ptr == NULL)
            return;
        
        std::string pid(ptr);
        if(pid.empty())
            return;
        
        std::string content=repo.read(pid);
        if(content.empty())
            codeBuffer -> text("Code not found\n");
        else
            codeBuffer -> text(content.c_str());
    }

    void onAddEdit()
    {
        if(!addWindow)
        {
            addWindow=new Fl_Window(500,400,"Add/Edit Code");
            addIdInput=new Fl_Input(100,20,380,25);

            new Fl_Box(20,20,80,25,"Code ID:");
            new Fl_Box(20,60,100,25,"Code Content:");

            addContentInput=new Fl_Multiline_Input(20,90,460,250);

            Fl_Button *btnAddSave=new Fl_Button(150,350,80,30,"Save");
            Fl_Button *btnAddCancel=new Fl_Button(250,350,80,30,"Cancel");

            btnAddSave -> callback(cb_AddSave,this);
            btnAddCancel -> callback(cb_AddCancel,this);

            addWindow -> callback(cb_AddCancel,this);
            addWindow -> end();
        }

        addIdInput -> value("");
        addContentInput -> value("");
        addWindow -> set_modal();
        addWindow -> show();
        addIdInput -> take_focus();
    }

    void onRemove() 
    {
        const char *ptr=fl_input("Enter the code ID to read:", "");
        if(ptr == NULL)
            return;

        std::string pid(ptr);
        if(pid.empty())
            return;
        
        bool ok=repo.remove(pid);
        if(ok)
            fl_message("Code snippet \"%s\" removed.",pid.c_str());
        else
            fl_alert("Code not found.");
    }

    void onAddSave()
    {
        std::string pid=addIdInput -> value();
        std::string content=addContentInput -> value();
        if(pid.empty())
        {
            fl_alert("Code ID cannot be empty.");
            return;
        }
        
        // slit content into lines
        std::vector<std::string> lines;

        std::istringstream iss(content);
        std::string line;
        while(std::getline(iss,line))
            lines.push_back(line);

        try
        {
            repo.add(pid, lines);
        }
        catch(const std::exception& e)
        {
            fl_alert("%s",e.what());
            return;
        }

        addWindow -> hide();
    }

    void onAddCancel()
    {
        if(addWindow)
            addWindow -> hide();
    }

    void onGuess()
    {
        if(!game)
            return;

        std::string guess=guessInput -> value();
        guessInput -> value("");  // clear input for next guess
        if(guess.empty())
            return;
        
        std::vector<std::string> msg=game -> makeGuess(guess);

        if(game -> isOver())
        {
            fl_alert("%s",game -> Lose().c_str());
            cleanupGame();
            return;
        }
        if(game -> isFinished())
        {
            fl_message("%s",game -> Win().c_str());
            cleanupGame();
            return;
        }
        
        updateGameDisplay(msg);
        
        guessInput -> take_focus();
    }

    void onAuto()
    {
        if(!game)
            return;
        
        std::string suggestion=autoGuesser.guess(game -> getMasked());
        updateGameDisplay(std::vector<std::string>{suggestion});
        
        guessInput -> take_focus();
    }

    void onGiveUp()
    {
        if(!game)
        {
            if(gameWindow) 
                gameWindow -> hide();
            
            mainWindow -> show();
            return;
        }

        fl_alert("%s",game -> Lose().c_str());
        cleanupGame();
    }

    void updateGameDisplay(const std::vector<std::string>& messages)
    {
        if (!game || !gameBuffer) 
            return;

        std::vector<std::string> lines=game -> getDisplayLines();
        
        // remove console instruction line for original UI
        if(!lines.empty())
        {
            const std::string& lastLine=lines.back();
            if(lastLine.rfind("Enter",0) == 0)
            {
                lines.pop_back();
            }
        }

        std::string text;
        for(const std::string& line:lines)
            text += line+'\n';
        for (const std::string& msg:messages)
            text += msg +'\n';

        gameBuffer -> text(text.c_str());
    }

    void cleanupGame()
    {
        if(game)
        {
            delete game;
            game=nullptr;
        }

        stats.saveToFile();
    
        gameWindow -> hide();
        mainWindow -> show();
    }
};

int main()
{
    //UI ui;
    //ui.mainloop();
    //return 0;
    GUI gui;
    return Fl::run();
}
