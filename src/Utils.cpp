#include "lcc.hpp"
#include <cctype>

namespace lcc
{
    bool isSpace(const char c)
    {
        return (c == ' ' || c == '\t' || c == '\f' || c == '\v');
    }

    bool isLetter(const char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    bool isDigit(const char c)
    {
        return std::isdigit(c);
    }

    json jsonifyTokens(const std::vector<std::shared_ptr<Token>> &tokens)
    {
        json arr = json::array();
        for (auto &token : tokens)
        {
            json j;
            switch (token->type)
            {
            case TokenType::TOKEN_NEWLINE:
                break;
            case TokenType::TOKEN_IDENTIFIER:
                j["id"] = token->count;
                j["type"] = "TOKEN_IDENTIFIER";
                j["content"] = token->content;
                j["position"] = {token->pos.line, token->pos.column};
                break;
            case TokenType::TOKEN_EOF:
                j["id"] = token->count;
                j["type"] = "TOKEN_EOF";
                j["content"] = "EOF";
                j["position"] = {token->pos.line, token->pos.column};
                break;
            case TokenType::TOKEN_WHITESPACE:
                break;
            case TokenType::TOKEN_INVALID:
                break;
            case TokenType::TOKEN_STRING:
                j["id"] = token->count;
                j["type"] = "TOKEN_STRING";
                j["content"] = token->content;
                j["position"] = {token->pos.line, token->pos.column};
                break;
            case TokenType::TOKEN_INTEGER:
                j["id"] = token->count;
                j["type"] = "TOKEN_INTEGER";
                j["content"] = token->content;
                j["position"] = {token->pos.line, token->pos.column};
                break;
            case TokenType::TOKEN_FLOAT:
                j["id"] = token->count;
                j["type"] = "TOKEN_FLOAT";
                j["content"] = token->content;
                j["position"] = {token->pos.line, token->pos.column};
                break;
            case TokenType::TOKEN_CHAR:
                j["id"] = token->count;
                j["type"] = "TOKEN_CHAR";
                j["content"] = token->content;
                j["position"] = {token->pos.line, token->pos.column};
                break;
#define keyword(name, disc)                                   \
    case TokenType::name:                                     \
        j["id"] = token->count;                               \
        j["type"] = #name;                                    \
        j["content"] = disc;                                  \
        j["position"] = {token->pos.line, token->pos.column}; \
        break;
#define punctuator(name, disc) keyword(name, disc)
#include "TokenType.inc"
#undef punctuator
#undef keyword

            default:
                break;
            }
            if (!j.empty())
                arr.emplace_back(j);
        }
        return arr;
    }

    bool dumpJson(const json &j, const std::string outPath)
    {
        if (outPath.empty())
        {
            FATAL_ERROR("Dump file path not specified.");
            return false;
        }

        std::ofstream ofs(outPath);
        ofs << j.dump(2) << std::endl;
        ofs.close();
        return true;
    }
}