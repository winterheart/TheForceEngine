#include "tfs_compiler.h"
#include "tfs_lexer.h"
#include "tfs_vm.h"
#include "tfs_hashTable.h"
#include "tfs_object.h"
#include <TFE_System/system.h>
#include <TFE_FrontEndUI/console.h>
#include <assert.h>
#include <vector>

/*
Implementation TODOs:
* Clean up implementation.
* Use resizable arrays, memory areas, etc.
* Implement/integrate editor + debugger.
* Seperate compilation from running.
* Clean up so multiple scripts can be compiled and ran.
* Look into "NAN" boxing. With this floats might become doubles?
  See https://www.craftinginterpreters.com/optimization.html

Language TODOs:
<First pass, basic language>
* [Done] Native functions.
* [Done] Structs.
===================================
<Finish core language>
* Finish grammar (++, --, +=, -=, etc.)
* Native memory (share variables between native and scripts).
* Remove need to ';' separator in most cases.
* Add 'var' or 'type' requirement for function declaration parameters.
* Add optional type specification for variables.
* Multi-pass compilation to allow for global variable indices, imports, etc.
  * Add #import keyword to import new files.
  * #import pass; structure pass; global variable pass; function pass; compilation pass.
* Namespaces.
* yield()/wait() intrinsics.
* vec2, vec3, vec4 intrinsics.
* Scheduler, handle multiple scripts running at once with instances.
===================================
<TFE Integration>
* Serialization (include script state).
* Convert existing scripts.
* Performance testing.
* Conversion to register based compilation and testing.
* Verify performance is competitive with Angel Script.
* Finalize.
*/

namespace TFE_ForceScript
{
	using namespace TFS_VM;
	using namespace TFS_HashTable;

	#define MAX_LOCAL_COUNT 256
	#define MAX_FUNC_ARG 255

	namespace TFS_Compiler
	{
		typedef void(*ParseFunc)(bool);

		// Lowest to highest.
		enum Precedence
		{
			PREC_NONE = 0,
			PREC_ASSIGNMENT,  // =
			PREC_OR,          // ||
			PREC_AND,         // &&
			PREC_EQUALITY,    // == !=
			PREC_COMPARISON,  // < > <= >=
			PREC_TERM,        // + -
			PREC_FACTOR,      // * /
			PREC_UNARY,       // ! -
			PREC_CALL,        // . ()
			PREC_PRIMARY
		};

		struct ParseRule
		{
			ParseFunc prefix;
			ParseFunc infix;
			Precedence precedence;
		};

		struct Local
		{
			Token name;
			s32 depth;
		};

		enum FuncType
		{
			TYPE_FUNC = 0,
			TYPE_SCRIPT,
			TYPE_COUNT
		};

		struct Compiler
		{
			Compiler* enclosing;

			ObjFunc* func;
			FuncType type;

			Local locals[MAX_LOCAL_COUNT];
			s32 localCount;
			s32 scopeDepth;
		};

		std::vector<char> s_stringBuffer;
		static CodeBlock* s_compilingBlock;

		u8 makeConstant(Value value);
		u8 makeConstant(const char* buffer, s32 length);
		bool match(s32 type);
		void declaration();
		bool identifiersEqual(Token* a, Token* b);
		void statement();
		void beginScope();
		void endScope();
		void varDeclaration();
		void expressionStatement();

		u8 parseVariable(const char* errorMessage);
		void defineVariable(u8 globalId);
		void namedVariable(Token* name, bool canAssign);
		
		// Add reserved words to the token list.
		enum ReservedWords
		{
			TOKEN_ELSE = TOKEN_EOF + 1,
			TOKEN_IF,
			TOKEN_OR,
			TOKEN_PRINT,
			TOKEN_RETURN,
			TOKEN_STRUCT,
			TOKEN_VAR,
			TOKEN_WHILE,
			TOKEN_TRUE,
			TOKEN_FALSE,
			TOKEN_FOR,
		};
				
		void number(bool canAssign);
		void string(bool canAssign);
		void variable(bool canAssign);
		void literal(bool canAssign);
		void grouping(bool canAssign);
		void call(bool canAssign);
		void dot(bool canAssign);
		void unary(bool canAssign);
		void unaryPost(bool canAssign);
		void binary(bool canAssign);
		void and_(bool canAssign);
		void or_(bool canAssign);

		static const ParseRule c_parseRules[] =
		{
			{nullptr, nullptr, PREC_NONE}, // TOKEN_UNKNOWN,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_PARSE_ERROR,
			{grouping, call,   PREC_CALL}, // TOKEN_OPEN_PAREN,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_CLOSE_PAREN,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_COLON,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_SEMICOLON,
			{nullptr, binary,  PREC_FACTOR}, // TOKEN_ASTERISK,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_COMMA,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_OPEN_BRACKET,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_CLOSE_BRACKET,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_OPEN_BRACE,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_CLOSE_BRACE,
			{nullptr, binary,  PREC_TERM}, // TOKEN_PLUS,
			{unary,   binary,  PREC_TERM}, // TOKEN_MINUS,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_AND,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_OR,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_EQ,
			{unary,   nullptr, PREC_NONE}, // TOKEN_NOT,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_XOR,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_MOD,
			{nullptr, binary,  PREC_FACTOR}, // TOKEN_FORWARD_SLASH,
			{nullptr, binary,  PREC_COMPARISON}, // TOKEN_LESS,
			{nullptr, binary,  PREC_COMPARISON}, // TOKEN_GREATER,
			{nullptr, dot,     PREC_CALL}, // TOKEN_PERIOD,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_QUESTION,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_POUND,

			{nullptr, binary,  PREC_EQUALITY}, // TOKEN_EQEQ,
			{nullptr, binary,  PREC_EQUALITY}, // TOKEN_NOTEQ,
			{nullptr, binary,  PREC_COMPARISON}, // TOKEN_LESSEQ,
			{nullptr, binary,  PREC_COMPARISON}, // TOKEN_GREATEREQ,
			{nullptr, and_,    PREC_AND}, // TOKEN_ANDAND,
			{nullptr, or_,     PREC_OR},  // TOKEN_OROR,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_SHL,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_SHR,
			{unary,   nullptr, PREC_NONE}, // TOKEN_INCR,
			{unary,   nullptr, PREC_NONE}, // TOKEN_DECR,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_MULEQ,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_DIVEQ,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_ADDEQ,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_SUBEQ,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_MODEQ,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_ANDEQ,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_OREQ,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_XOREQ,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_SHLEQ,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_SHREQ,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_ARROW,

			{variable,nullptr, PREC_NONE}, // TOKEN_IDENTIFIER,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_CHAR_LITERAL,
			{string,  nullptr, PREC_NONE}, // TOKEN_STRING_LITERAL,
			{number,  nullptr, PREC_NONE}, // TOKEN_FLOAT_LITERAL,
			{number,  nullptr, PREC_NONE}, // TOKEN_INT_LITERAL,

			{nullptr, nullptr, PREC_NONE}, // TOKEN_EOF,

			{nullptr, nullptr, PREC_NONE}, // TOKEN_ELSE
			{nullptr, nullptr, PREC_NONE}, // TOKEN_IF,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_OR,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_PRINT,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_RETURN
			{nullptr, nullptr, PREC_NONE}, // TOKEN_STRUCT
			{nullptr, nullptr, PREC_NONE}, // TOKEN_VAR,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_WHILE,
			{literal, nullptr, PREC_NONE}, // TOKEN_TRUE,
			{literal, nullptr, PREC_NONE}, // TOKEN_FALSE,
			{nullptr, nullptr, PREC_NONE}, // TOKEN_FOR,
		};

		static Compiler* s_context = nullptr;

		s32 checkKeyword(s32 start, s32 length, s32 lstart, s32 lend, const char* value, const char* rest, s32 type)
		{
			if (lend - lstart + 1 == start + length && memcmp(value + start, rest, length) == 0)
			{
				return type;
			}
			return TOKEN_IDENTIFIER;
		}

		s32 getTokenIdentifierType(const char* value, s32 start, s32 end)
		{
			switch (value[0])
			{
			case 'e': return checkKeyword(1, 3, start, end, value, "lse", TOKEN_ELSE);
			case 'f':
			{
				if (end - start + 1 > 1)
				{
					switch (value[1])
					{
					case 'a': return checkKeyword(2, 3, start, end, value, "lse", TOKEN_FALSE);
					case 'o': return checkKeyword(2, 1, start, end, value, "r", TOKEN_FOR);
					}
				}
			} break;
			case 'i': return checkKeyword(1, 1, start, end, value, "f", TOKEN_IF);
			case 'p': return checkKeyword(1, 4, start, end, value, "rint", TOKEN_PRINT);
			case 'r': return checkKeyword(1, 5, start, end, value, "eturn", TOKEN_RETURN);
			case 's': return checkKeyword(1, 5, start, end, value, "truct", TOKEN_STRUCT);
			case 't': return checkKeyword(1, 3, start, end, value, "rue", TOKEN_TRUE);
			case 'v': return checkKeyword(1, 2, start, end, value, "ar", TOKEN_VAR);
			case 'w': return checkKeyword(1, 4, start, end, value, "hile", TOKEN_WHILE);
			}

			return TOKEN_IDENTIFIER;
		}

		struct Parser
		{
			Token cur;
			Token prev;

			bool panicMode = false;
			bool hadError = false;

			s32 errorCount = 0;
		};
		static Parser s_parser = {};

		void errorAt(const Token* token, const char* msg)
		{
			// Do not continue spewing out errors while in panic mode.
			if (s_parser.panicMode) { return; }

			TokenLocation loc;
			TFS_Lexer::getSourceAddress(token->start, &loc);

			if (token->type == TOKEN_PARSE_ERROR)
			{
				TFE_Console::print("/cff4040 [line %d] Parsing Error at %d \"%s\"", loc.lineNumber, loc.lineOffset + 1, TFS_Lexer::getString(s_parser.cur.start, s_parser.cur.end));
			}
			else
			{
				TFE_Console::print("/cff4040 [line %d] Error at %d \"%s\" : %s", loc.lineNumber, loc.lineOffset + 1, TFS_Lexer::getString(s_parser.cur.start, s_parser.cur.end), msg);
			}

			s_parser.hadError = true;
			s_parser.panicMode = true;
			s_parser.errorCount++;
		}

		void errorAtCurrent(const char* msg)
		{
			errorAt(&s_parser.cur, msg);
		}

		void error(const char* msg)
		{
			errorAt(&s_parser.prev, msg);
		}

		bool advance()
		{
			s_parser.prev = s_parser.cur;
			s_parser.cur = *TFS_Lexer::getToken();

			if (s_parser.cur.type == TOKEN_PARSE_ERROR)
			{
				errorAtCurrent("Parse Error");
				return false;
			}
			else if (s_parser.cur.type == TOKEN_IDENTIFIER)
			{
				const char* value = TFS_Lexer::getString(s_parser.cur.start, s_parser.cur.end);
				s_parser.cur.type = (TokenType)getTokenIdentifierType(value, s_parser.cur.start, s_parser.cur.end);
			}
			return true;
		}

		void consume(s32 type, const char* msg)
		{
			if (s_parser.cur.type == type)
			{
				advance();
				return;
			}
			errorAtCurrent(msg);
		}

		void emitByte(u8 byte)
		{
			TFS_VM::writeCode(s_compilingBlock, byte);
		}

		void emitBytes(u8 a, u8 b)
		{
			emitByte(a);
			emitByte(b);
		}

		void startCompiler(Compiler* compiler, FuncType type)
		{
			compiler->enclosing = s_context;
			compiler->func = allocateFunction();
			compiler->localCount = 0;
			compiler->scopeDepth = 0;
			s_compilingBlock = &compiler->func->code;

			// Local variable on the stack to hold function name.
			Local* local = &compiler->locals[compiler->localCount++];
			local->depth = 0;
			local->name.start = 0;
			local->name.len = 0;

			s_context = compiler;

			if (type != TYPE_SCRIPT)
			{
				s_context->func->name = copyString(s_parser.prev.sValue, s_parser.prev.len);
			}
		}

		void emitReturn()
		{
			emitByte(OP_NULL);
			emitByte(OP_RET);
		}

		ObjFunc* endCompiler()
		{
			emitReturn();

			ObjFunc* func = s_context->func;

			s_context = s_context->enclosing;
			if (s_context && s_context->func)
			{
				s_compilingBlock = &s_context->func->code;
			}

			return func;
		}

		const ParseRule* getRule(s32 type)
		{
			return &c_parseRules[type];
		}

		void parsePrecedence(Precedence precedence)
		{
			if (!advance())
			{
				return;
			}

			ParseFunc prefixRule = getRule(s_parser.prev.type)->prefix;
			if (!prefixRule)
			{
				error("Expect expression.");
			}
			bool canAssign = precedence <= PREC_ASSIGNMENT;
			prefixRule(canAssign);

			while (precedence <= getRule(s_parser.cur.type)->precedence)
			{
				advance();
				ParseFunc infixRule = getRule(s_parser.prev.type)->infix;
				infixRule(canAssign);
			}

			if (canAssign && match(TOKEN_EQ))
			{
				error("Invalid assignment target.");
			}
		}

		bool check(s32 type)
		{
			return s_parser.cur.type == type;
		}

		bool match(s32 type)
		{
			if (!check(type)) { return false; }
			advance();
			return true;
		}

		void expression()
		{
			if (s_parser.panicMode)
			{
				return;
			}

			parsePrecedence(PREC_ASSIGNMENT);
		}

		void printStatement()
		{
			expression();
			// TODO: Remove the need to end with ';'
			consume(TOKEN_SEMICOLON, "Expect ';' after value.");
			emitByte(OP_PRINT);
		}

		s32 emitJump(u8 instr)
		{
			emitByte(instr);
			// 16 bit jump offset.
			emitByte(0xff);
			emitByte(0xff);
			return s_compilingBlock->size - 2;
		}

		void patchJump(s32 offset)
		{
			// -2 to adjust for the bytecode for the jump offset itself.
			s32 jump = s_compilingBlock->size - offset - 2;
			if (jump > UINT16_MAX)
			{
				error("Too much code to jump over.");
			}

			// encode 16 bit value
			s_compilingBlock->code[offset] = (jump >> 8) & 0xff;
			s_compilingBlock->code[offset + 1] = jump & 0xff;
		}

		void ifStatement()
		{
			// Do we want to require parenthesis here?
			consume(TOKEN_OPEN_PAREN, "Expect '(' after 'if'.");
			expression();
			consume(TOKEN_CLOSE_PAREN, "Expect ')' after condition.");

			s32 thenJump = emitJump(OP_JUMP_IF_FALSE);
			emitByte(OP_POP);
			statement();
			s32 elseJump = emitJump(OP_JUMP);
			patchJump(thenJump);
			emitByte(OP_POP);

			if (match(TOKEN_ELSE)) { statement(); }
			patchJump(elseJump);
		}

		void emitLoop(s32 loopStart)
		{
			emitByte(OP_LOOP);

			s32 offset = s_compilingBlock->size - loopStart + 2;
			if (offset > UINT16_MAX)
			{
				error("Loop body is too large.");
			}

			emitByte((offset >> 8) & 0xff);
			emitByte(offset);
		}

		void whileStatement()
		{
			// The beginning of the loop.
			s32 loopStart = s_compilingBlock->size;

			// Do we want to require parenthesis here?
			consume(TOKEN_OPEN_PAREN, "Expect '(' after 'if'.");
			expression();
			consume(TOKEN_CLOSE_PAREN, "Expect ')' after condition.");

			s32 exitJump = emitJump(OP_JUMP_IF_FALSE);
			emitByte(OP_POP);

			statement();
			emitLoop(loopStart);

			patchJump(exitJump);
			emitByte(OP_POP);
		}

		void forStatement()
		{
			beginScope();
			
			consume(TOKEN_OPEN_PAREN, "Expect '(' after 'for'.");

			// Initializer.
			if (match(TOKEN_SEMICOLON))
			{
				// no initializer.
			}
			else if (match(TOKEN_VAR))
			{
				varDeclaration();
			}
			else
			{
				expressionStatement();
			}

			// Conditional clause.
			s32 loopStart = s_compilingBlock->size;
			s32 exitJump = -1;
			if (!match(TOKEN_SEMICOLON))
			{
				expression();
				consume(TOKEN_SEMICOLON, "Expect ';'.");

				exitJump = emitJump(OP_JUMP_IF_FALSE);
				emitByte(OP_POP);
			}

			// Increment
			if (!match(TOKEN_CLOSE_PAREN))
			{
				s32 bodyJump = emitJump(OP_JUMP);

				s32 incrStart = s_compilingBlock->size;
				expression();
				emitByte(OP_POP);
				consume(TOKEN_CLOSE_PAREN, "Expected ')' after for clauses.");

				emitLoop(loopStart);
				loopStart = incrStart;
				patchJump(bodyJump);
			}
			
			// Body
			statement();

			// Loop Jump
			emitLoop(loopStart);

			if (exitJump != -1)
			{
				patchJump(exitJump);
				emitByte(OP_POP);
			}

			endScope();
		}

		// Expression statements evaluates an expression and then discards the results.
		void expressionStatement()
		{
			expression();
			consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
			emitByte(OP_POP);
		}
		
		void beginScope()
		{
			s_context->scopeDepth++;
		}

		void endScope()
		{
			s_context->scopeDepth--;

			// Pop local variables off the stack until there are no more at this depth.
			while (s_context->localCount > 0 && s_context->locals[s_context->localCount - 1].depth > s_context->scopeDepth)
			{
				emitByte(OP_POP);
				s_context->localCount--;
			}
		}

		void block()
		{
			while (!check(TOKEN_CLOSE_BRACE) && !check(TOKEN_EOF))
			{
				declaration();
			}
			consume(TOKEN_CLOSE_BRACE, "Expect '}' after block.");
		}

		void function(FuncType type)
		{
			Compiler compiler;
			startCompiler(&compiler, type);
			beginScope();

			// Compile the parameter list.
			consume(TOKEN_OPEN_PAREN, "Expect '(' after function name.");
			if (!check(TOKEN_CLOSE_PAREN))
			{
				do
				{
					s_context->func->arity++;
					if (s_context->func->arity > MAX_FUNC_ARG)
					{
						errorAtCurrent("Can't have more than 255 parameters.");
					}

					u8 paramConst = parseVariable("Expect parameter name.");
					defineVariable(paramConst);
				} while (match(TOKEN_COMMA));
			}
			consume(TOKEN_CLOSE_PAREN, "Expect ')' after function name.");

			// The body.
			consume(TOKEN_OPEN_BRACE, "Expect '{' before function body.");
			block();

			// Create the function object.
			ObjFunc* func = endCompiler();
			emitBytes(OP_CONSTANT, makeConstant(objValue((Object*)func)));
		}

		void returnStatement()
		{
			if (s_context->type == TYPE_SCRIPT)
			{
				error("Can't return from top-level code.");
			}

			if (match(TOKEN_SEMICOLON))
			{
				emitReturn();
			}
			else
			{
				expression();
				consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
				emitByte(OP_RET);
			}
		}

		void statement()
		{
			if (match(TOKEN_PRINT))
			{
				printStatement();
			}
			else if (match(TOKEN_IF))
			{
				ifStatement();
			}
			else if (match(TOKEN_RETURN))
			{
				returnStatement();
			}
			else if (match(TOKEN_WHILE))
			{
				whileStatement();
			}
			else if (match(TOKEN_FOR))
			{
				forStatement();
			}
			else if (match(TOKEN_OPEN_BRACE))
			{
				beginScope();
				block();
				endScope();
			}
			else
			{
				expressionStatement();
			}
		}

		u8 identifierConstant(Token* name)
		{
			return makeConstant(name->sValue, name->len);
		}

		s32 resolveLocal(Compiler* context, Token* name)
		{
			for (s32 i = context->localCount - 1; i >= 0; i--)
			{
				Local* local = &context->locals[i];
				if (identifiersEqual(name, &local->name))
				{
					if (local->depth < 0)
					{
						error("Can't read local variable in its own initializer.");
					}
					return i;
				}
			}

			return -1;
		}

		void addLocal(Token* name)
		{
			if (s_context->localCount >= MAX_LOCAL_COUNT)
			{
				error("Too many local variables in function.");
				return;
			}

			Local* local = &s_context->locals[s_context->localCount++];
			local->name = *name;
			local->depth = -1;
		}

		bool identifiersEqual(Token* a, Token* b)
		{
			return TFS_Lexer::tokenStringsMatch(a, b);
		}

		void declareVariable()
		{
			if (s_context->scopeDepth == 0) { return; }

			Token* name = &s_parser.prev;
			for (s32 i = s_context->localCount - 1; i >= 0; i--)
			{
				Local* local = &s_context->locals[i];
				if (local->depth != -1 && local->depth < s_context->scopeDepth)
				{
					break;
				}

				if (identifiersEqual(name, &local->name))
				{
					error("Local variable already exists in scope.");
				}
			}

			addLocal(name);
		}

		u8 parseVariable(const char* errorMessage)
		{
			consume(TOKEN_IDENTIFIER, errorMessage);

			declareVariable();
			if (s_context->scopeDepth > 0) { return 0; }

			return identifierConstant(&s_parser.prev);
		}

		void markInitialized()
		{
			if (s_context->scopeDepth == 0) { return; }
			s_context->locals[s_context->localCount - 1].depth = s_context->scopeDepth;
		}
		
		void defineVariable(u8 globalId)
		{
			if (s_context->scopeDepth > 0)
			{
				markInitialized();
				return;
			}

			emitBytes(OP_DEFINE_GLOBAL, globalId);
		}

		void varDeclaration()
		{
			u8 global = parseVariable("Expect variable name.");

			if (match(TOKEN_EQ))
			{
				expression();
			}
			else
			{
				emitByte(OP_NULL);
			}
			consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

			defineVariable(global);
		}

		void structField(s32 offset)
		{
			consume(TOKEN_VAR, "Expect struct field declaration (such as 'var').");
			consume(TOKEN_IDENTIFIER, "Expect field name.");
			u8 constant = identifierConstant(&s_parser.prev);
			emitBytes(OP_CONSTANT, makeConstant(intValue(offset)));
			emitBytes(OP_FIELD, constant);
			consume(TOKEN_SEMICOLON, "Expect ';' after declaration.");
		}

		void structDeclaration()
		{
			consume(TOKEN_IDENTIFIER, "Expect struct name.");
			Token structName = s_parser.prev;
			u8 nameConstant = identifierConstant(&s_parser.prev);
			declareVariable();

			emitBytes(OP_STRUCT, nameConstant);
			defineVariable(nameConstant);

			namedVariable(&structName, false);
			consume(TOKEN_OPEN_BRACE, "Expect '{' before struct body.");
			s32 offset = 0;
			while (!check(TOKEN_CLOSE_BRACE) && !check(TOKEN_EOF))
			{
				structField(offset);
				offset++;
			}
			consume(TOKEN_CLOSE_BRACE, "Expect '}' after struct body.");
			emitByte(OP_POP);
		}

		// Try to find a safe place to resume parsing in order to output multiple errors.
		void synchronize()
		{
			s_parser.panicMode = false;

			while (s_parser.cur.type != TOKEN_EOF)
			{
				// TODO: use 'end of line' seperator when possible.
				if (s_parser.prev.type == TOKEN_SEMICOLON) return;

				switch (s_parser.cur.type)
				{
					case TOKEN_STRUCT:
					case TOKEN_VAR:
					case TOKEN_FOR:
					case TOKEN_IF:
					case TOKEN_WHILE:
					case TOKEN_PRINT:
					case TOKEN_RETURN:
						return;
				}

				advance();
			}
		}

		void funcDecl()
		{
			u8 global = parseVariable("Expect function name");
			markInitialized();

			function(TYPE_FUNC);
			defineVariable(global);
		}

		void declaration()
		{
			if (match(TOKEN_VAR))
			{
				varDeclaration();
			}
			else if (match(TOKEN_STRUCT))
			{
				structDeclaration();
			}
			// We can only declare functions at the root scope (no closures).
			else if (check(TOKEN_IDENTIFIER) && s_context->func->name == nullptr && s_context->scopeDepth == 0)
			{
				funcDecl();
			}
			else
			{
				statement();
			}

			if (s_parser.panicMode)
			{
				synchronize();
			}
		}

#define MAX_CONSTANT_COUNT 256

		u8 makeConstant(Value value)
		{
			s32 constant = TFS_VM::addConstant(s_compilingBlock, value);
			if (constant > MAX_CONSTANT_COUNT)
			{
				TFE_Console::print("/cff4040 Too many constants in one block.");
				return 0;
			}
			return u8(constant);
		}
				
		void emitConstant(f32 value)
		{
			emitBytes(OP_CONSTANT, makeConstant(floatValue(value)));
		}

		void emitConstant(s32 value)
		{
			emitBytes(OP_CONSTANT, makeConstant(floatValue(f32(value))));
		}
				
		void number(bool canAssign)
		{
			// For now force everything to be floats.
			if (s_parser.prev.type == TOKEN_INT_LITERAL)
			{
				emitConstant(s_parser.prev.iValue);
			}
			else
			{
				emitConstant(s_parser.prev.fValue);
			}
		}
				
		u8 makeConstant(const char* buffer, s32 length)
		{
			Value value;
			value.ovalue = (Object*)copyString(buffer, length);
			value.type = VALUE_OBJ;
			
			s32 constant = TFS_VM::addConstant(s_compilingBlock, value);
			if (constant > MAX_CONSTANT_COUNT)
			{
				TFE_Console::print("/cff4040 Too many constants in one block.");
				return 0;
			}
			return u8(constant);
		}

		void string(bool canAssign)
		{
			const char* str = TFS_Lexer::getString(s_parser.prev.start, s_parser.prev.end);
			const s32 length = s_parser.prev.end - s_parser.prev.start + 1;
			emitBytes(OP_CONSTANT, makeConstant(str, length));
		}

		void namedVariable(Token* name, bool canAssign)
		{
			u8 getOp, setOp;
			s32 arg = resolveLocal(s_context, name);
			if (arg != -1)
			{
				getOp = OP_GET_LOCAL;
				setOp = OP_SET_LOCAL;
			}
			else
			{
				arg = identifierConstant(name);
				getOp = OP_GET_GLOBAL;
				setOp = OP_SET_GLOBAL;
			}

			if (canAssign && match(TOKEN_EQ))
			{
				expression();
				emitBytes(setOp, (u8)arg);
			}
			else if (canAssign && match(TOKEN_ADDEQ))
			{
				emitBytes(getOp, (u8)arg);
				expression();
				emitByte(OP_ADD);
				emitBytes(setOp, (u8)arg);
			}
			else if (canAssign && match(TOKEN_SUBEQ))
			{
				emitBytes(getOp, (u8)arg);
				expression();
				emitByte(OP_SUB);
				emitBytes(setOp, (u8)arg);
			}
			else if (canAssign && match(TOKEN_MULEQ))
			{
				emitBytes(getOp, (u8)arg);
				expression();
				emitByte(OP_MUL);
				emitBytes(setOp, (u8)arg);
			}
			else if (canAssign && match(TOKEN_DIVEQ))
			{
				emitBytes(getOp, (u8)arg);
				expression();
				emitByte(OP_DIV);
				emitBytes(setOp, (u8)arg);
			}
			// TODO: Incr, decr have the wrong precedence.
			else if (canAssign && match(TOKEN_INCR))
			{
				emitBytes(getOp, (u8)arg);
				emitByte(OP_INC);
				emitBytes(setOp, (u8)arg);
			}
			else if (canAssign && match(TOKEN_DECR))
			{
				emitBytes(getOp, (u8)arg);
				emitByte(OP_DEC);
				emitBytes(setOp, (u8)arg);
			}
			else
			{
				emitBytes(getOp, (u8)arg);
			}
		}

		void modifyNamedVariable(Token* name, u8 op)
		{
			u8 setOp;
			s32 arg = resolveLocal(s_context, name);
			if (arg != -1)
			{
				setOp = OP_SET_LOCAL;
			}
			else
			{
				arg = identifierConstant(name);
				setOp = OP_SET_GLOBAL;
			}

			emitByte(op);
			emitBytes(setOp, (u8)arg);
		}

		void variable(bool canAssign)
		{
			namedVariable(&s_parser.prev, canAssign);
		}

		void literal(bool canAssign)
		{
			switch (s_parser.prev.type)
			{
				case TOKEN_FALSE: emitByte(OP_FALSE); break;
				case TOKEN_TRUE: emitByte(OP_TRUE); break;
				default:
					return;
			}
		}

		void grouping(bool canAssign)
		{
			expression();
			consume(TOKEN_CLOSE_PAREN, "Expect ')' after expression.");
		}

		u8 argumentList()
		{
			u8 argCount = 0;
			if (!check(TOKEN_CLOSE_PAREN))
			{
				do
				{
					expression();
					if (argCount == MAX_FUNC_ARG)
					{
						error("Can't have more than 255 arguments.");
					}
					argCount++;
				} while (match(TOKEN_COMMA));
			}

			consume(TOKEN_CLOSE_PAREN, "Expect ')' after function arguments.");
			return argCount;
		}

		void call(bool canAssign)
		{
			u8 argCount = argumentList();
			emitBytes(OP_CALL, argCount);
		}

		void dot(bool canAssign)
		{
			consume(TOKEN_IDENTIFIER, "Expect field name after '.'.");
			u8 name = identifierConstant(&s_parser.prev);

			if (canAssign && match(TOKEN_EQ))
			{
				expression();
				emitBytes(OP_SET_FIELD, name);
			}
			else
			{
				emitBytes(OP_GET_FIELD, name);
			}
		}

		void unary(bool canAssign)
		{
			s32 type = s_parser.prev.type;

			// Compile the operand.
			parsePrecedence(PREC_UNARY);

			// Emit the operator instruction.
			switch (type)
			{
				case TOKEN_MINUS: emitByte(OP_NEGATE); break;
				case TOKEN_NOT: emitByte(OP_NOT); break;
				case TOKEN_INCR: modifyNamedVariable(&s_parser.prev, OP_INC); break;
				case TOKEN_DECR: modifyNamedVariable(&s_parser.prev, OP_DEC); break;
				default:
					return;
			}
		}

		void binary(bool canAssign)
		{
			s32 type = s_parser.prev.type;

			// Compile the right operand.
			const ParseRule* rule = getRule(type);
			parsePrecedence(Precedence(rule->precedence + 1));

			// Emit the operator instruction.
			switch (type)
			{
				case TOKEN_PLUS: emitByte(OP_ADD); break;
				case TOKEN_MINUS: emitByte(OP_SUB); break;
				case TOKEN_ASTERISK: emitByte(OP_MUL); break;
				case TOKEN_FORWARD_SLASH: emitByte(OP_DIV); break;
				case TOKEN_EQEQ: emitByte(OP_EQ); break;
				case TOKEN_NOTEQ: emitByte(OP_NOT_EQ); break;
				case TOKEN_LESS: emitByte(OP_LESS); break;
				case TOKEN_LESSEQ: emitByte(OP_LESS_EQ); break;
				case TOKEN_GREATER: emitByte(OP_GREATER); break;
				case TOKEN_GREATEREQ: emitByte(OP_GREATER_EQ); break;
				default:
					return;
			}
		}

		void and_(bool canAssign)
		{
			s32 endJump = emitJump(OP_JUMP_IF_FALSE);

			emitByte(OP_POP);
			parsePrecedence(PREC_AND);

			patchJump(endJump);
		}

		void or_(bool canAssign)
		{
			s32 endJump = emitJump(OP_JUMP_IF_TRUE);

			emitByte(OP_POP);
			parsePrecedence(PREC_OR);

			patchJump(endJump);
		}
		
		bool compile(const char* buffer, size_t len)
		{
			s_stringBuffer.resize(1024);
			TFS_Lexer::init(buffer, len, s_stringBuffer.data(), s_stringBuffer.size());
		
			memset(&s_parser, 0, sizeof(Parser));
			TFS_VM::init();

			Compiler compiler;
			startCompiler(&compiler, TYPE_SCRIPT);

			// prime the parser.
			advance();
			// compile declarations until we reach the end.
			while (!match(TOKEN_EOF))
			{
				declaration();
			}
			// the result is a function that can be run.
			ObjFunc* func = endCompiler();

			if (s_parser.hadError)
			{
				TFE_Console::print("/cff4040 Script \"%s\" had %d errors.", "DevScript", s_parser.errorCount);
			}
			else
			{
				TFS_VM::execute(func);
			}

			// Later this should be freed until we don't need to run anymore.
			TFS_VM::destroy();

			return s_parser.hadError;
		}
	}
}