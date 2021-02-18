#include "tfs_lexer.h"
#include <TFE_System/system.h>

namespace TFE_ForceScript
{
namespace TFS_Lexer
{
	static const char* s_inputStream;
	static const char* s_eof;
	static const char* s_parsePoint;
	static char* s_stringBuffer;
	static size_t s_stringBufferSize;

	static Token s_token;

	Token* setToken(TokenType type, const char* start, const char* end)
	{
		s_token.type = type;
		s_token.start = u32(start - s_inputStream);
		s_token.end = u32(end - s_inputStream);

		s_parsePoint = end + 1;
		return &s_token;
	}

	Token* setEndOfFile()
	{
		s_token.type = TOKEN_EOF;
		s_token.start = 0;
		s_token.end = 0;
		return &s_token;
	}

	void init(const char* buffer, size_t bufferLen, char* stringBuffer, size_t bufferSize)
	{
		s_inputStream = buffer;
		s_eof = s_inputStream + bufferLen;
		s_parsePoint = s_inputStream;

		s_stringBuffer = stringBuffer;
		s_stringBufferSize = bufferSize;
	}

	bool isWhiteSpace(char ch)
	{
		return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f';
	}

	s32 parseChar(const char* p, const char** ptr)
	{
		if (*p == '\\')
		{
			*ptr = p + 2;
			switch (p[1])
			{
			case '\\': return '\\';
			case '\'': return '\'';
			case '"': return '"';
			case 't': return '\t';
			case 'f': return '\f';
			case 'n': return '\n';
			case 'r': return '\r';
			case '0': return '\0';	// TODO: octal constants.
			case 'x': case 'X': return -1;	// TODO: hex constants.
			case 'u': return -1;	// TODO: unicode constants.
			}
		}
		*ptr = p + 1;
		return (u8)(*p);
	}

	bool identifierStart(char ch)
	{
		return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || (unsigned char)ch >= 128;
	}

	// Allow digits in the middle or end of an identifier.
	bool identifierMiddle(char ch)
	{
		return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || (unsigned char)ch >= 128;
	}

	const Token* parseString(const char* p)
	{
		const char* start = p;
		char delim = *p;
		p++;

		char* out = s_stringBuffer;
		char* outEnd = s_stringBuffer + s_stringBufferSize;

		while (*p != delim)
		{
			s32 n;
			if (*p == '\\')
			{
				const char* q;
				n = parseChar(p, &q);
				if (n < 0)
				{
					return setToken(TOKEN_PARSE_ERROR, start, q);
				}
				p = q;
			}
			else
			{
				n = (u8)(*p);
				p++;
			}

			if (out + 1 > outEnd)
			{
				return setToken(TOKEN_PARSE_ERROR, start, p);
			}
			*out = (char)n;
			out++;
		}
		*out = 0;
		s_token.sValue = s_stringBuffer;
		s_token.len = s32(out - s_stringBuffer);
		return setToken(TOKEN_STRING_LITERAL, start, p);
	}

	static char s_tmpString[1024];
	char* getString(s32 start, s32 end)
	{
		if (start < 0 || start >= s32(s_eof - s_inputStream))
		{
			strcpy(s_tmpString, "error");
			return s_tmpString;
		}

		s_tmpString[0] = 0;
		for (s32 i = start; i <= end; i++)
		{
			s_tmpString[i - start] = s_inputStream[i];
		}
		s_tmpString[end - start + 1] = 0;
		return s_tmpString;
	}

	bool tokenStringsMatch(const Token* a, const Token* b)
	{
		const s32 lenA = a->end - a->start + 1;
		const s32 lenB = b->end - b->start + 1;
		if (lenA != lenB) { return false; }

		return memcmp((void*)&s_inputStream[a->start], (void*)&s_inputStream[b->start], lenA) == 0;
	}

	bool getSourceAddress(s32 index, TokenLocation* loc)
	{
		if (index < 0 || index >= s32(s_eof - s_inputStream) || !loc)
		{
			return false;
		}

		const char* p = s_inputStream;
		s32 lineNumber = 1;
		s32 charOffset = 0;
		const char* where = p + index;

		while (*p && p < where)
		{
			if (*p == '\n')
			{
				lineNumber++;
				p++;
				charOffset = 0;
			}
			else if (*p == '\r')
			{
				p++;
				charOffset = 0;
			}
			else if (*p == '\t')
			{
				p++;
				charOffset += 4;
			}
			else
			{
				p++;
				charOffset++;
			}
		}

		loc->lineNumber = lineNumber;
		loc->lineOffset = charOffset;
		return true;
	}
				
	const Token* getToken()
	{
		const char* p = s_parsePoint;
		// Skip whitespace and comments.
		for (;;)
		{
			while (p != s_eof && isWhiteSpace(*p))
			{
				p++;
			}

			// C++ style comments.
			if (p != s_eof && p[0] == '/' && p[1] == '/')
			{
				while (p != s_eof && *p != '\r' && *p != '\n')
				{
					++p;
				}
				continue;
			}

			// C style comments.
			if (p != s_eof && p[0] == '/' && p[1] == '*')
			{
				const char* start = p;
				p += 2;
				while (p != s_eof && (p[0] != '*' || p[1] != '/'))
				{
					++p;
				}
				if (p == s_eof)
				{
					return setToken(TOKEN_PARSE_ERROR, start, p - 1);
				}

				p += 2;
				continue;
			}

			break;
		}
				
		if (p == s_eof)
		{
			return setEndOfFile();
		}

		switch (*p)
		{
			case '+':
				if (p + 1 != s_eof)
				{
					if (p[1] == '+') { return setToken(TOKEN_INCR, p, p+1); }
					if (p[1] == '=') { return setToken(TOKEN_ADDEQ, p, p + 1); }
				}
				return setToken(TOKEN_PLUS, p, p);
				break;
			case '-':
				if (p + 1 != s_eof)
				{
					if (p[1] == '-') { return setToken(TOKEN_DECR, p, p + 1); }
					if (p[1] == '=') { return setToken(TOKEN_SUBEQ, p, p + 1); }
					if (p[1] == '>') { return setToken(TOKEN_ARROW, p, p + 1); }
				}
				return setToken(TOKEN_MINUS, p, p);
				break;
			case '&':
				if (p + 1 != s_eof)
				{
					if (p[1] == '&') { return setToken(TOKEN_ANDAND, p, p + 1); }
					if (p[1] == '=') { return setToken(TOKEN_ANDEQ, p, p + 1); }
				}
				return setToken(TOKEN_AND, p, p);
				break;
			case '|':
				if (p + 1 != s_eof)
				{
					if (p[1] == '|') { return setToken(TOKEN_OROR, p, p + 1); }
					if (p[1] == '=') { return setToken(TOKEN_OREQ, p, p + 1); }
				}
				return setToken(TOKEN_OR, p, p);
				break;
			case '=':
				if (p + 1 != s_eof)
				{
					if (p[1] == '=') { return setToken(TOKEN_EQEQ, p, p + 1); }
				}
				return setToken(TOKEN_EQ, p, p);
				break;
			case '!':
				if (p + 1 != s_eof)
				{
					if (p[1] == '=') { return setToken(TOKEN_NOTEQ, p, p + 1); }
				}
				return setToken(TOKEN_NOT, p, p);
				break;
			case '^':
				if (p + 1 != s_eof)
				{
					if (p[1] == '=') { return setToken(TOKEN_XOREQ, p, p + 1); }
				}
				return setToken(TOKEN_XOR, p, p);
				break;
			case '%':
				if (p + 1 != s_eof)
				{
					if (p[1] == '=') { return setToken(TOKEN_MODEQ, p, p + 1); }
				}
				return setToken(TOKEN_MOD, p, p);
				break;
			case '*':
				if (p + 1 != s_eof)
				{
					if (p[1] == '=') { return setToken(TOKEN_MULEQ, p, p + 1); }
				}
				return setToken(TOKEN_ASTERISK, p, p);
				break;
			case '/':
				if (p + 1 != s_eof)
				{
					if (p[1] == '=') { return setToken(TOKEN_DIVEQ, p, p + 1); }
				}
				return setToken(TOKEN_FORWARD_SLASH, p, p);
				break;
			case '<':
				if (p + 1 != s_eof)
				{
					if (p[1] == '=') { return setToken(TOKEN_LESSEQ, p, p + 1); }
					if (p[1] == '<')
					{
						if (p + 2 != s_eof && p[2] == '=')
						{
							return setToken(TOKEN_SHLEQ, p, p + 2);
						}
						return setToken(TOKEN_SHL, p, p + 1);
					}
				}
				return setToken(TOKEN_LESS, p, p);
				break;
			case '>':
				if (p + 1 != s_eof)
				{
					if (p[1] == '=') { return setToken(TOKEN_GREATEREQ, p, p + 1); }
					if (p[1] == '>')
					{
						if (p + 2 != s_eof && p[2] == '=')
						{
							return setToken(TOKEN_SHREQ, p, p + 2);
						}
						return setToken(TOKEN_SHR, p, p + 1);
					}
				}
				return setToken(TOKEN_GREATER, p, p);
				break;
			case '"':
				return parseString(p);
				break;
			case '\'':
			{
				const char* start = p;
				s_token.iValue = parseChar(p + 1, &p);
				if (s_token.iValue < 0)
				{
					return setToken(TOKEN_PARSE_ERROR, start, start);
				}
				if (p == s_eof || *p != '\'')
				{
					return setToken(TOKEN_PARSE_ERROR, start, p);
				}
				return setToken(TOKEN_CHAR_LITERAL, start, p + 1);
			} break;
			case '.':
				if (p + 1 != s_eof)
				{
					// This is a float in the form: .123
					if (*(p + 1) >= '0' && *(p + 1) <= '9')
					{
						const char* q;
						s_token.fValue = (f32)strtod(p, (char**)&q);
						return setToken(TOKEN_FLOAT_LITERAL, p, q - 1);
					}
				}
				return setToken(TOKEN_PERIOD, p, p);
				break;
			case '(':
				return setToken(TOKEN_OPEN_PAREN, p, p);
				break;
			case ')':
				return setToken(TOKEN_CLOSE_PAREN, p, p);
				break;
			case '[':
				return setToken(TOKEN_OPEN_BRACKET, p, p);
				break;
			case ']':
				return setToken(TOKEN_CLOSE_BRACKET, p, p);
				break;
			case '{':
				return setToken(TOKEN_OPEN_BRACE, p, p);
				break;
			case '}':
				return setToken(TOKEN_CLOSE_BRACE, p, p);
				break;
			case ',':
				return setToken(TOKEN_COMMA, p, p);
				break;
			case '#':
				return setToken(TOKEN_POUND, p, p);
				break;
			case ';':
				return setToken(TOKEN_SEMICOLON, p, p);
				break;
			case ':':
				return setToken(TOKEN_COLON, p, p);
				break;
			case '?':
				return setToken(TOKEN_QUESTION, p, p);
				break;
			case '0':
				if (p + 1 != s_eof)
				{
					// HEX literal
					if (p[1] == 'x' || p[1] == 'X')
					{
						char* q;
						s_token.iValue = strtol(p, &q, 16);

						if (q == p + 2)
						{
							return setToken(TOKEN_PARSE_ERROR, p-2, p-1);
						}
						return setToken(TOKEN_INT_LITERAL, p, q-1);
					}
				}
			// Fallthrough, in cases where 0 is used but it is NOT a HEX literal.
			case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			{
				const char* q = p;
				while (q != s_eof && (*q >= '0' && *q <= '9')) { q++; }

				if (q != s_eof && *q == '.')
				{
					s_token.fValue = (f32)strtod(p, (char**)&q);
					return setToken(TOKEN_FLOAT_LITERAL, p, q-1);
				}

				s_token.iValue = strtol(p, (char**)&q, 10);
				return setToken(TOKEN_INT_LITERAL, p, q-1);
			} break;

			default:
			{
				if (identifierStart(*p))
				{
					s32 n = 0;
					s_token.sValue = s_stringBuffer;
					s_token.len = 0;
					do
					{
						s_stringBuffer[n] = p[n];
						n++;
					} while (identifierMiddle(p[n]));
					s_stringBuffer[n] = 0;
					s_token.len = n;
					return setToken(TOKEN_IDENTIFIER, p, p + n - 1);
				}
			}
		}

		return setToken(TOKEN_PARSE_ERROR, p, p);
	}

	static const char* c_tokenTable[]=
	{
		"Unknown", // TOKEN_UNKNOWN = 0,
		"Parse Error", // TOKEN_PARSE_ERROR,
		"(", // TOKEN_OPEN_PAREN,
		")", // TOKEN_CLOSE_PAREN,
		":", // TOKEN_COLON,
		";", // TOKEN_SEMICOLON,
		"*", // TOKEN_ASTERISK,
		",", // TOKEN_COMMA,
		"[", // TOKEN_OPEN_BRACKET,
		"]", // TOKEN_CLOSE_BRACKET,
		"{", // TOKEN_OPEN_BRACE,
		"}", // TOKEN_CLOSE_BRACE,
		"+", // TOKEN_PLUS,
		"-", // TOKEN_MINUS,
		"&", // TOKEN_AND,
		"|", // TOKEN_OR,
		"=", // TOKEN_EQ,
		"~", // TOKEN_NOT,
		"^", // TOKEN_XOR,
		"%", // TOKEN_MOD,
		"/", // TOKEN_FORWARD_SLASH,
		"<", // TOKEN_LESS,
		">", // TOKEN_GREATER,
		".", // TOKEN_PERIOD,
		"?", // TOKEN_QUESTION,
		"#", // TOKEN_POUND,

		"==", // TOKEN_EQEQ,
		"!=", // TOKEN_NOTEQ,
		"<=", // TOKEN_LESSEQ,
		">=", // TOKEN_GREATEREQ,
		"&&", // TOKEN_ANDAND,
		"||", // TOKEN_OROR,
		"<<", // TOKEN_SHL,
		">>", // TOKEN_SHR,
		"++", // TOKEN_INCR,
		"--", // TOKEN_DECR,
		"*=", // TOKEN_MULEQ,
		"/=", // TOKEN_DIVEQ,
		"+=", // TOKEN_ADDEQ,
		"-=", // TOKEN_SUBEQ,
		"%=", // TOKEN_MODEQ,
		"&=", // TOKEN_ANDEQ,
		"|=", // TOKEN_OREQ,
		"^=", // TOKEN_XOREQ,
		"<<=", // TOKEN_SHLEQ,
		">>=", // TOKEN_SHREQ,
		"->", // TOKEN_ARROW,

		"Identifier", // TOKEN_IDENTIFIER,
		"CharLiteral", // TOKEN_CHAR_LITERAL,
		"StringLiteral", // TOKEN_STRING_LITERAL,
		"FloatLiteral", // TOKEN_FLOAT_LITERAL,
		"IntLiteral", // TOKEN_INT_LITERAL,

		"EOF", // TOKEN_EOF,
	};
	
	void printToken(const Token* token)
	{
		char output[4096];
		if (token->type == TOKEN_IDENTIFIER)
		{
			sprintf(output, "%s = %s ", c_tokenTable[token->type], token->sValue);
		}
		else if (token->type == TOKEN_CHAR_LITERAL)
		{
			sprintf(output, "%s = '%c' ", c_tokenTable[token->type], (char)token->iValue);
		}
		else if (token->type == TOKEN_STRING_LITERAL)
		{
			sprintf(output, "%s = \"%s\" ", c_tokenTable[token->type], token->sValue);
		}
		else if (token->type == TOKEN_FLOAT_LITERAL)
		{
			sprintf(output, "%s = %f ", c_tokenTable[token->type], token->fValue);
		}
		else if (token->type == TOKEN_INT_LITERAL)
		{
			sprintf(output, "%s = %d ", c_tokenTable[token->type], token->iValue);
		}
		else
		{
			sprintf(output, "%s ", c_tokenTable[token->type]);
		}

		TFE_System::logWrite(LOG_MSG, "DEBUG", output);
	}
}
}