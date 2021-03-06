/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2012 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
// Check for assignment / condition mismatches
//---------------------------------------------------------------------------

#include "checkassignif.h"
#include "symboldatabase.h"

//---------------------------------------------------------------------------

// Register this check class (by creating a static instance of it)
namespace {
    CheckAssignIf instance;
}


void CheckAssignIf::assignIf()
{
    if (!_settings->isEnabled("style"))
        return;

    for (const Token *tok = _tokenizer->tokens(); tok; tok = tok->next()) {
        if (tok->str() != "=")
            continue;

        if (Token::Match(tok->tokAt(-2), "[;{}] %var% = %var% [&|] %num% ;")) {
            const unsigned int varid(tok->previous()->varId());
            if (varid == 0)
                continue;

            const char bitop(tok->strAt(2).at(0));

            const MathLib::bigint num = MathLib::toLongNumber(tok->strAt(3));
            if (num < 0)
                continue;

            for (const Token *tok2 = tok->tokAt(4); tok2; tok2 = tok2->next()) {
                if (tok2->str() == "(" || tok2->str() == "}" || tok2->str() == "=")
                    break;
                if (Token::Match(tok2,"if ( %varid% %any% %num% &&|%oror%|)", varid)) {
                    const std::string& op(tok2->strAt(3));
                    const MathLib::bigint num2 = MathLib::toLongNumber(tok2->strAt(4));
                    if (op == "==" && (num & num2) != ((bitop=='&') ? num2 : num))
                        assignIfError(tok2, false);
                    else if (op == "!=" && (num & num2) != ((bitop=='&') ? num2 : num))
                        assignIfError(tok2, true);
                    break;
                }
            }
        }
    }
}

void CheckAssignIf::assignIfError(const Token *tok, bool result)
{
    reportError(tok, Severity::style,
                "assignIfError",
                "Mismatching assignment and comparison, comparison is always " + std::string(result ? "true" : "false") + ".");
}





void CheckAssignIf::comparison()
{
    if (!_settings->isEnabled("style"))
        return;

    for (const Token *tok = _tokenizer->tokens(); tok; tok = tok->next()) {
        if (Token::Match(tok, "&|%or% %num% )| ==|!= %num% &&|%oror%|)")) {
            const MathLib::bigint num1 = MathLib::toLongNumber(tok->strAt(1));
            if (num1 < 0)
                continue;

            const Token *compareToken = tok->tokAt(2);
            if (compareToken->str() == ")") {
                if (!Token::Match(compareToken->link()->previous(), "(|%oror%|&&"))
                    continue;
                compareToken = compareToken->next();
            }

            const MathLib::bigint num2 = MathLib::toLongNumber(compareToken->strAt(1));
            if (num2 < 0)
                continue;

            if ((tok->str() == "&" && (num1 & num2) != num2) ||
                (tok->str() == "|" && (num1 | num2) != num2)) {
                const std::string& op(compareToken->str());
                comparisonError(tok, tok->str(), num1, op, num2, op=="==" ? false : true);
            }
        }
    }
}

void CheckAssignIf::comparisonError(const Token *tok, const std::string &bitop, MathLib::bigint value1, const std::string &op, MathLib::bigint value2, bool result)
{
    std::ostringstream expression;
    expression << std::hex << "(X " << bitop << " 0x" << value1 << ") " << op << " 0x" << value2;

    const std::string errmsg("Expression '" + expression.str() + "' is always " + (result?"true":"false") + ".\n"
                             "The expression '" + expression.str() + "' is always " + (result?"true":"false") +
                             ". Check carefully constants and operators used, these errors might be hard to "
                             "spot sometimes. In case of complex expression it might help to split it to "
                             "separate expressions.");

    reportError(tok, Severity::style, "comparisonError", errmsg);
}






void CheckAssignIf::multiCondition()
{
    if (!_settings->isEnabled("style"))
        return;

    const SymbolDatabase* const symbolDatabase = _tokenizer->getSymbolDatabase();

    for (std::list<Scope>::const_iterator i = symbolDatabase->scopeList.begin(); i != symbolDatabase->scopeList.end(); ++i) {
        if (i->type == Scope::eIf && Token::Match(i->classDef, "if ( %var% & %num% ) {")) {
            const Token* const tok = i->classDef;
            const unsigned int varid(tok->tokAt(2)->varId());
            if (varid == 0)
                continue;

            const MathLib::bigint num1 = MathLib::toLongNumber(tok->strAt(4));
            if (num1 < 0)
                continue;

            const Token *tok2 = tok->linkAt(6);
            while (Token::simpleMatch(tok2, "} else { if (")) {
                // Goto '('
                const Token * const opar = tok2->tokAt(4);

                // tok2: skip if-block
                tok2 = opar->link();
                if (Token::simpleMatch(tok2, ") {"))
                    tok2 = tok2->next()->link();

                // check condition..
                if (Token::Match(opar, "( %varid% ==|& %num% &&|%oror%|)", varid)) {
                    const MathLib::bigint num2 = MathLib::toLongNumber(opar->strAt(3));
                    if (num2 < 0)
                        continue;

                    if ((num1 & num2) == num2) {
                        multiConditionError(opar, tok->linenr());
                    }
                }
            }
        }
    }
}

void CheckAssignIf::multiConditionError(const Token *tok, unsigned int line1)
{
    std::ostringstream errmsg;
    errmsg << "Expression is always false because 'else if' condition matches previous condition at line "
           << line1 << ".";

    reportError(tok, Severity::style, "multiCondition", errmsg.str());
}
