#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <vector>
#include <string>

namespace TFE_ForceScript
{
	enum TokenType
	{
		TOKEN_UNKNOWN = 0,
		TOKEN_PARSE_ERROR,
		// Single character
		TOKEN_OPEN_PAREN,
		TOKEN_CLOSE_PAREN,
		TOKEN_COLON,
		TOKEN_SEMICOLON,
		TOKEN_ASTERISK,
		TOKEN_COMMA,
		TOKEN_OPEN_BRACKET,
		TOKEN_CLOSE_BRACKET,
		TOKEN_OPEN_BRACE,
		TOKEN_CLOSE_BRACE,
		TOKEN_PLUS,
		TOKEN_MINUS,
		TOKEN_AND,
		TOKEN_OR,
		TOKEN_EQ,
		TOKEN_NOT,
		TOKEN_XOR,
		TOKEN_MOD,
		TOKEN_FORWARD_SLASH,
		TOKEN_LESS,
		TOKEN_GREATER,
		TOKEN_PERIOD,
		TOKEN_QUESTION,
		TOKEN_POUND,
		
		TOKEN_EQEQ,
		TOKEN_NOTEQ,
		TOKEN_LESSEQ,
		TOKEN_GREATEREQ,
		TOKEN_ANDAND,
		TOKEN_OROR,
		TOKEN_SHL,
		TOKEN_SHR,
		TOKEN_INCR,
		TOKEN_DECR,
		TOKEN_MULEQ,
		TOKEN_DIVEQ,
		TOKEN_ADDEQ,
		TOKEN_SUBEQ,
		TOKEN_MODEQ,
		TOKEN_ANDEQ,
		TOKEN_OREQ,
		TOKEN_XOREQ,
		TOKEN_SHLEQ,
		TOKEN_SHREQ,
		TOKEN_ARROW,

		TOKEN_IDENTIFIER,
		TOKEN_CHAR_LITERAL,
		TOKEN_STRING_LITERAL,
		TOKEN_FLOAT_LITERAL,
		TOKEN_INT_LITERAL,

		TOKEN_EOF,
	};

	struct Token
	{
		TokenType type;
		u32 start;
		u32 end;

		union
		{
			s32 iValue;
			f32 fValue;
			const char* sValue;
		};
		s32 len;
	};

	struct TokenLocation
	{
		s32 lineNumber;
		s32 lineOffset;
	};

	namespace TFS_Lexer
	{
		void init(const char* buffer, size_t bufferLen, char* stringBuffer, size_t bufferSize);
		const Token* getToken();

		// Get the address in the source based on an index.
		bool getSourceAddress(s32 index, TokenLocation* loc);
		char* getString(s32 start, s32 end);
		bool tokenStringsMatch(const Token* a, const Token* b);

		void printToken(const Token* token);
	}
}