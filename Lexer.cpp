#include "lcc.hpp"

namespace cc
{
    Lexer::Lexer(File* file):
        _file(file), _tokenCnt(0)
    {
        #define keyword(name, disc) _keywordMap.insert(std::make_pair(disc, TokenType::name));
        #define operator(name, disc) 
        #include "TokenType.inc"
        #undef operator
        #undef keyword

        nextLine();
    }

    void Lexer::nextLine()
    {
        _file->nextLine();
    }

    const char Lexer::nextChar()
    {
        return _file->nextChar();
    }

    bool Lexer::isNextChar(const char c)
    {
        if(_file->peekChar() == c) 
        {
            nextChar();
            return true;
        }

        return false;
    }

    const char Lexer::peekChar()
    {
        return _file->peekChar();
    }

    void Lexer::retractChar()
    {
        _file->retractChar();
    }

    bool Lexer::ignoreSpaces()
    {
        if(!isSpace(peekChar()))
            return false;

        do
        {
            nextChar();
        } while (isSpace(peekChar()));
        
        return true;
    }

    void Lexer::ignoreComments()
    {
        if(peekChar() != '/') return;

        nextChar();
        if(isNextChar('/')) // ignore line comment
        {
            char c = nextChar();
            while(c != '\n' && c != EOF) c = nextChar();
            retractChar();
        }
        else if(isNextChar('*')) // ignore block comment
        {
            bool isLastStar = false;
            char c = 0;
            do
            {
                c = nextChar();
                if(c == '\n') nextLine();
                if(c == '/' && isLastStar) break;
                isLastStar = (c == '*');
            } while (c != EOF);
        }
        else retractChar();
    }

    void Lexer::run()
    {
        json j = json::array();
        while(true)
        {
            Token* token = nextToken();
            json tmp;
            switch (token->type)
            {
            case TokenType::TOKEN_NEWLINE:
                break;
            case TokenType::TOKEN_IDENTIFIER:
                tmp["id"] = token->count;
                tmp["type"] = "TOKEN_IDENTIFIER";
                tmp["content"] = token->pchar;
                tmp["position"] = {token->pos.line, token->pos.column};
                j.push_back(tmp);
                break;
            case TokenType::TOKEN_EOF:
                break;
            case TokenType::TOKEN_WHITESPACE:
                break;
            case TokenType::TOKEN_INVALID:
                break;
            case TokenType::TOKEN_KEYWORD:
                tmp["id"] = token->count;
                tmp["type"] = "TOKEN_KEYWORD";
                tmp["content"] = (std::string("") + (char)token->id).c_str();
                tmp["position"] = {token->pos.line, token->pos.column};
                j.push_back(tmp);
                break;
            case TokenType::TOKEN_STRING:
            case TokenType::TOKEN_NUMBER:
            case TokenType::TOKEN_CHAR:
                break;
            #define keyword(name, disc) \
            case TokenType::name: \
                tmp["id"] = token->count; \
                tmp["type"] = #name; \
                tmp["content"] = disc; \
                tmp["position"] = {token->pos.line, token->pos.column}; \
                j.push_back(tmp); \
                break;
            #define operator(name, disc) keyword(name, disc)
            #include "TokenType.inc"
            #undef operator
            #undef keyword

            default:
                break;
            }
            if(token->type == TokenType::TOKEN_EOF) break;
        }

        std::cout << j.dump(1) << std::endl;
    }

    Token* Lexer::makeGeneralToken(Token token) const
    {
        Token* pToken = new Token(std::move(token));
        pToken->pos = _curTokenPos;
        pToken->file = _file;
        pToken->count = _tokenCnt;
        return pToken;
    }

    Token* Lexer::makeSpaceToken() const
    {
        Token token;
        token.type = TokenType::TOKEN_WHITESPACE;
        return makeGeneralToken(token);
    }

    Token* Lexer::makeEOFToken() const
    {
        Token token;
        token.type = TokenType::TOKEN_EOF;
        return makeGeneralToken(token);
    }

    Token* Lexer::makeNewlineToken() const
    {
        Token token;
        token.type = TokenType::TOKEN_NEWLINE;
        return makeGeneralToken(token);
    }

    Token* Lexer::makeInvalidToken() const
    {
        Token token;
        token.type = TokenType::TOKEN_INVALID;
        return makeGeneralToken(token);
    }

    Token* Lexer::makeIdentifierToken(CharBuffer& buffer) const
    {
        Token token;
        token.type = TokenType::TOKEN_IDENTIFIER;
        token.pchar = buffer.new_c_str();
        token.length = buffer.size();
        return makeGeneralToken(token);
    }

    Token* Lexer::makeCharToken(CharBuffer& buffer)
    {
        Token token;
        token.type = TokenType::TOKEN_CHAR;
        token.pchar = buffer.new_c_str();
        token.length = buffer.size();
        return makeGeneralToken(token);
    }

    Token* Lexer::forwardSearch(const char possibleCh, TokenType possibleType, TokenType defaultType)
    {
        if(isNextChar(possibleCh)) return makeKeywordToken(possibleType);

        return makeKeywordToken(defaultType);
    }

    Token* Lexer::forwardSearch(const char possibleCh1, TokenType possibleType1, const char possibleCh2, TokenType possibleType2, TokenType defaultType)
    {
        if(isNextChar(possibleCh1)) return makeKeywordToken(possibleType1);
        else if(isNextChar(possibleCh2)) return makeKeywordToken(possibleType2);

        return makeKeywordToken(defaultType);
    }   

    Token* Lexer::readIdentifier(char c)
    {
        CharBuffer buffer;
        buffer.append(c);
        while(true)
        {
            c = nextChar();
            if(isalnum(c) || (c & 0x80) || c == '_' || c == '$') // numbders and ascii letters are accepted
            {
                buffer.append(c);
                continue;
            }
            retractChar();

            // determine whether this identifier is a keyword
            for(auto& pair: _keywordMap)
            {
                if(buffer == pair.first) return makeKeywordToken(pair.second);
            }

            return makeIdentifierToken(buffer);
        }
    }

    Token* Lexer::readString()
    {
        CharBuffer buffer;
        while(true)
        {
            char c = nextChar();
            if(c == '\"') break;
            else if(c == '\\') {
                c = nextChar();
            }
            buffer.append(c);
        }

        return makeStringToken(buffer);
    }

    Token* Lexer::readNumber(char c)
    {
        CharBuffer buffer;
        buffer.append(c);
        while(true)
        {
            c = nextChar();
            if(c < '0' || c > '9')
            {
                retractChar();
                break;
            }
            buffer.append(c);
        }

        return makeNumberToken(buffer);
    }

    Token* Lexer::readChar()
    {
        CharBuffer buffer;
        while(true)
        {
            char c = nextChar();
            if(c == '\'') break;
            else if(c == '\\') c = nextChar();
            buffer.append(c);
        }

        return makeCharToken(buffer);
    }

    Token* Lexer::makeKeywordToken(TokenType keywordType) const
    {
        Token token;
        token.type = keywordType;
        return makeGeneralToken(token);
    }

    Token* Lexer::makeKeywordToken(int id) const
    {
        Token token;
        token.type = TokenType::TOKEN_KEYWORD;
        token.id = id;
        return makeGeneralToken(token);
    }

    Token* Lexer::makeStringToken(CharBuffer& buffer) const
    {
        Token token;
        token.type = TokenType::TOKEN_STRING;
        token.pchar = buffer.new_c_str();
        token.length = buffer.size();
        return makeGeneralToken(token);
    }

    Token* Lexer::makeNumberToken(CharBuffer& buffer) const
    {
        Token token;
        token.type = TokenType::TOKEN_NUMBER;
        token.pchar = buffer.new_c_str();
        token.length = buffer.size();
        return makeGeneralToken(token);
    }

    Token* Lexer::nextToken()
    {
        ignoreComments();
        _curTokenPos = _file->getPosition();
        if(ignoreSpaces()) return makeSpaceToken();
        char ch = nextChar();
        _tokenCnt++;
        switch(ch)
        {
        case '\n':
        {
            Token* token = makeNewlineToken();
            nextLine();
            return token;
        }
        case 'a' ... 'z': case 'A' ... 'Z': case '_': case '$':
        case 0x80 ... 0xFD:
            return readIdentifier(ch);
        case '0' ... '9':
            return readNumber(ch);
        case '=': return forwardSearch('=', TokenType::TOKEN_OPEQ, TokenType::TOKEN_OPASSIGN);
        case '<': return forwardSearch('=', TokenType::TOKEN_OPLEQ, TokenType::TOKEN_OPLESS);
        case '>': return forwardSearch('=', TokenType::TOKEN_OPGEQ, TokenType::TOKEN_OPGREATER);
        case '+': return makeKeywordToken(TokenType::TOKEN_OPADD);
        case '-': return makeKeywordToken(TokenType::TOKEN_OPMINUS);
        case '*': return makeKeywordToken(TokenType::TOKEN_OPTIMES);
        case '/': return makeKeywordToken(TokenType::TOKEN_OPDIV);
        case '{': case '}': 
        case '[': case ']':
        case '(': case ')': // this isn't the best way to deal with brackets!
        case ';':
            return makeKeywordToken((int)ch);
        case '\"':
            return readString();
        case '\'':
            return readChar();
        case EOF: 
            return makeEOFToken();
        default:
            return makeInvalidToken();
        }
    }
}